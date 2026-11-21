import gdb

class IdtGate(gdb.Command):
    """idt-gate VEC[IDT_BASE]: DECODE 64-BIT IDT GATE"""

    def __init__(self):
        super().__init__("idt-gate", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        argv = gdb.string_to_argv(arg)
        if not argv:
            raise gdb.GdbError("use idt-gate VEC [IDT_BASE]")
        vec = int(gdb.parse_and_eval(argv[0]))
        base = int(
            gdb.parse_and_eval(argv[1] if len(argv) > 1 else "(unsigned long)&IDT")
        )
        raw = bytes(gdb.selected_inferior().read_memory(base + vec * 0x10, 0x10))
        off_lo = int.from_bytes(raw[0:2], "little")
        sel = int.from_bytes(raw[2:4], "little")
        ist = raw[4] & 7
        attr = raw[5]
        off_mid = int.from_bytes(raw[6:8], "little")
        off_hi = int.from_bytes(raw[8:12], "little")
        handler = off_hi << 0x20 | off_mid << 0x10 | off_lo
        gdb.write(
            f"vec {vec}: handler=0x{handler:x} self=0x{sel:x} "
            f"ist={ist} attr=0x{attr:02x}\n"
        )
        gdb.write(f" -> {gdb.execute(f'info symbol 0x{handler:x}', to_string=True)}")


IdtGate()


class KStackTop(gdb.Function):
    def __init__(self):
        super().__init__("nmi_top")

    def invoke(self):
        return gdb.parse_and_eval("(unsigned long)&nmi_stack + sizeof(nmi_stack)")


KStackTop()


class DumpRegisters(gdb.Command):
    """Dump key 64-bit CPU registers cleanly"""

    def __init__(self):
        super().__init__("dump-regs", gdb.COMMAND_USER)

    def invoke(self, arg, from_tty):
        regs = ["rip", "rsp", "rbp", "rdi", "rsi", "rdx", "rcx", "rax", "gs", "fs", "cs"]
        for reg in regs:
            try:
                # Use parse_and_eval with explicit frame or fallback to gdb.Value conversion
                val_str = gdb.execute(f"print/x ${reg}", to_string=True).strip()
                # Output looks like "$1 = 0x100000" -> extract the value part
                if "=" in val_str:
                    val_hex = val_str.split("=")[1].strip()
                    gdb.write(f"  {reg.upper():<4} = {val_hex}\n")
                else:
                    gdb.write(f"  {reg.upper():<4} = {val_str}\n")
            except Exception:
                gdb.write(f"  {reg.upper():<4} = <unavailable>\n")

        # Try to resolve symbol for RIP
        try:
            rip_val = gdb.parse_and_eval("$rip")
            rip = int(rip_val)
            sym = gdb.execute(f"info symbol 0x{rip:x}", to_string=True).strip()
            gdb.write(f"  -> {sym}\n")
        except Exception:
            pass


DumpRegisters()

class LxTasks(gdb.Command):
    """lx-ts [ID]: list kernel tasks (walks the task_list list_head), '*' marks
    current. With ID, show only that task plus its saved stack top."""
    STATE_NAMES = ["TASK_READY", "TASK_RUNNING", "TASK_BLOCKED", "TASK_DEAD"]

    def __init__(self):
        super().__init__("lx-ts", gdb.COMMAND_DATA)

    @staticmethod
    def _eval(*exprs):
        for e in exprs:
            try:
                return gdb.parse_and_eval(e)
            except gdb.error:
                continue
        return None

    def _state_map(self):
        m = {}
        for name in self.STATE_NAMES:
            try:
                m[int(gdb.parse_and_eval(name))] = name.replace("TASK_", "")
            except gdb.error:
                pass
        return m

    @staticmethod
    def _field(t, name, fmt="0x{:x}"):
        try:
            return fmt.format(int(t[name]))
        except (gdb.error, RuntimeError):
            return "?"

    @staticmethod
    def _sym(addr):
        try:
            out = gdb.execute(f"info symbol 0x{addr:x}", to_string=True).strip()
            return "" if out.startswith("No symbol") else out.split(" in ")[0]
        except gdb.error:
            return ""

    @classmethod
    def _tasks(cls):
        """Yield struct task * for every task on task_list, then idle_task
        (which is never linked)."""
        head = cls._eval("task_list", "'task.c'::task_list")
        if head is None:
            raise gdb.GdbError("task_list not found (is task.c built with -g?)")
        stype = gdb.lookup_type("struct task")
        off = next(f.bitpos for f in stype.fields() if f.name == "list") // 8
        ptype = stype.pointer()
        head_addr = int(head.address)
        node = int(head["next"])
        seen = set()
        while node and node != head_addr and len(seen) < 256:
            if node in seen:
                gdb.write("  !! cycle detected in task_list\n")
                break
            seen.add(node)
            t = gdb.Value(node - off).cast(ptype)
            yield t
            node = int(t["list"]["next"])
        idle = cls._eval("idle_task", "'task.c'::idle_task")
        if idle is not None:
            yield idle.address

    def invoke(self, arg, from_tty):
        argv = gdb.string_to_argv(arg)
        want = int(gdb.parse_and_eval(argv[0])) if argv else None
        cur = self._eval("current")
        cur_addr = int(cur) if cur is not None else 0
        states = self._state_map()

        gdb.write(f"  {'':1} {'ID':>3} {'STATE':<8} {'TASK':<18} {'CR3':<12} "
                  f"{'SAVED_RSP':<18} ENTRY\n")
        count = 0
        for t in self._tasks():
            task = t.dereference()
            addr = int(t)
            tid = int(task["id"])
            if want is not None and want != tid:
                continue
            count += 1
            mark = "*" if addr == cur_addr else " "
            st = int(task["state"])
            rsp = int(task["saved_rsp"])
            try:
                e = int(task["entry"])
                entry = self._sym(e) if e else ""
            except (gdb.error, RuntimeError):
                entry = ""
            gdb.write(f"  {mark} {tid:>3} {states.get(st, str(st)):<8} "
                      f"0x{addr:<16x} {self._field(task, 'cr3'):<12} "
                      f"0x{rsp:<16x} {entry}\n")
            if want is not None:
                gdb.write(f"      kstack: base={self._field(task, 'kstack_base')}"
                          f" top={self._field(task, 'kstack_top')}\n")
                # a task that is not running resumes at the return address
                # switch_to will pop: saved_rsp + 6 saved regs * 8
                try:
                    ra = int(gdb.parse_and_eval(
                        f"*(unsigned long *)(0x{rsp:x} + 48)"))
                    gdb.write(f"      resumes at 0x{ra:x} {self._sym(ra)}\n")
                except gdb.error:
                    pass
        if count == 0:
            gdb.write("  no matching task\n")


LxTasks()


# Physical frame tracking (pmm.c)
#
#   frame-stat                 free_count vs. the bitmap, next-fit hint
#   frame-snap                 remember the bitmap now
#   frame-diff                 frames allocated/freed since frame-snap
#   frame-track on|off|report|clear
#                              log every pmm_alloc_frame*/pmm_free_frame,
#                              remember who allocated each live frame

FRAME_SIZE = 4096


def _ev(*exprs):
    for e in exprs:
        try:
            return gdb.parse_and_eval(e)
        except gdb.error:
            continue
    return None


def _read_bitmap():
    """Return (bitmap_bytes, nframes). One bit per frame, bit f&7 of byte f>>3."""
    bm = _ev("bitmap", "'pmm.c'::bitmap")
    if bm is None:
        raise gdb.GdbError("bitmap not found (is pmm.c built with -g?)")
    nframes = bm.type.sizeof          # PAGECOUNT
    nbytes = (nframes + 7) // 8       # only this much is meaningful
    raw = bytes(gdb.selected_inferior().read_memory(int(bm.address), nbytes))
    return raw, nframes


def _used(bm, f):
    return (bm[f >> 3] >> (f & 7)) & 1


def _free_count():
    v = _ev("free_count", "'pmm.c'::free_count")
    return int(v) if v is not None else None


def _ranges(frames):
    """[3,4,5,9] -> [(3,5),(9,9)]"""
    out = []
    for f in sorted(frames):
        if out and f == out[-1][1] + 1:
            out[-1] = (out[-1][0], f)
        else:
            out.append((f, f))
    return out


def _fmt_ranges(frames, limit=12):
    rs = _ranges(frames)
    parts = []
    for a, b in rs[:limit]:
        if a == b:
            parts.append(f"0x{a * FRAME_SIZE:x}")
        else:
            parts.append(f"0x{a * FRAME_SIZE:x}-0x{b * FRAME_SIZE:x} ({b - a + 1})")
    if len(rs) > limit:
        parts.append(f"... +{len(rs) - limit} more ranges")
    return ", ".join(parts)


def _refcount(f):
    v = _ev(f"frame_table[{f}].refcount", f"'pmm.c'::frame_table[{f}].refcount")
    return int(v) if v is not None else -1


class FrameStat(gdb.Command):
    """frame-stat: free_count vs. bitmap, and the next-fit hint."""

    def __init__(self):
        super().__init__("frame-stat", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        bm, n = _read_bitmap()
        used = sum(bin(b).count("1") for b in bm[: n // 8])
        used += sum(_used(bm, f) for f in range((n // 8) * 8, n))
        free_bitmap = n - used
        fc = _free_count()
        last = _ev("last_alloc_frame", "'pmm.c'::last_alloc_frame")
        gdb.write(f"  total frames      : {n}\n")
        gdb.write(f"  free (bitmap)     : {free_bitmap}\n")
        gdb.write(f"  free (free_count) : {fc}\n")
        if fc is not None and fc != free_bitmap:
            gdb.write(f"  !! MISMATCH: free_count - bitmap = {fc - free_bitmap}\n")
        if last is not None:
            gdb.write(f"  next-fit hint     : frame {int(last)} "
                      f"(0x{int(last) * FRAME_SIZE:x})\n")


FrameStat()


class FrameSnap(gdb.Command):
    """frame-snap: save the allocation bitmap, compare later with frame-diff."""
    snap = None   # (bitmap_bytes, nframes, free_count)

    def __init__(self):
        super().__init__("frame-snap", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        bm, n = _read_bitmap()
        FrameSnap.snap = (bm, n, _free_count())
        gdb.write(f"  snapshot taken, free_count={FrameSnap.snap[2]}\n")


FrameSnap()


class FrameDiff(gdb.Command):
    """frame-diff: frames allocated/freed since the last frame-snap."""

    def __init__(self):
        super().__init__("frame-diff", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        if FrameSnap.snap is None:
            raise gdb.GdbError("no snapshot: run frame-snap first")
        old, n, old_fc = FrameSnap.snap
        new, _ = _read_bitmap()
        gained, released = [], []
        for i, (a, b) in enumerate(zip(old, new)):
            x = a ^ b
            if not x:
                continue
            for bit in range(8):
                if x >> bit & 1:
                    f = i * 8 + bit
                    if f >= n:
                        continue
                    (gained if b >> bit & 1 else released).append(f)
        fc = _free_count()
        gdb.write(f"  free_count: {old_fc} -> {fc}  (delta {None if fc is None or old_fc is None else fc - old_fc})\n")
        gdb.write(f"  allocated since snapshot: {len(gained)}\n")
        if gained:
            gdb.write(f"    {_fmt_ranges(gained)}\n")
            shown = sorted(gained)[:32]
            refs = {f: _refcount(f) for f in shown}
            nz = [f"0x{f * FRAME_SIZE:x}:ref={r}" for f, r in refs.items() if r > 0]
            if nz:
                gdb.write(f"    refcounted: {', '.join(nz)}\n")
        gdb.write(f"  freed since snapshot    : {len(released)}\n")
        if released:
            gdb.write(f"    {_fmt_ranges(released)}\n")
        gdb.write(f"  net leaked frames       : {len(gained) - len(released)}\n")


FrameDiff()


# allocation tracking

class _Track:
    live = {}          # phys addr -> "caller<-caller<-..."
    bps = []
    allocs = 0
    frees = 0
    untracked_frees = 0
    untracked = []     # (addr, caller chain) for frees we never saw allocated


def _callers(frame, depth=5):
    names = []
    while frame is not None and len(names) < depth:
        n = frame.name()
        names.append(n if n else f"0x{frame.pc():x}")
        frame = frame.older()
    return "<-".join(names)


class _AllocDone(gdb.FinishBreakpoint):
    def __init__(self, frame, who):
        super().__init__(frame, internal=True)
        self.who = who

    def stop(self):
        rv = self.return_value
        pa = int(rv) if rv is not None else 0
        if pa:
            _Track.live[pa] = self.who
            _Track.allocs += 1
        return False

    def out_of_scope(self):
        pass


class _AllocBP(gdb.Breakpoint):
    def stop(self):
        try:
            fr = gdb.newest_frame()
            _AllocDone(fr, _callers(fr.older()))
        except gdb.error:
            pass
        return False


class _FreeBP(gdb.Breakpoint):
    def stop(self):
        # SysV ABI: first argument is in rdi at the very first instruction
        # (installed with "*pmm_free_frame"), independent of debug info.
        try:
            fr = gdb.newest_frame()
            pa = int(fr.read_register("rdi")) & 0xFFFFFFFFFFFFFFFF
        except (gdb.error, ValueError):
            return False
        if _Track.live.pop(pa, None) is not None:
            _Track.frees += 1
        else:
            _Track.untracked_frees += 1
            if len(_Track.untracked) < 64:
                _Track.untracked.append((pa, _callers(fr.older(), 4)))
        return False


class FrameTrack(gdb.Command):
    """frame-track on|off|report|clear
    on     : break (silently) on pmm_alloc_frame, pmm_alloc_frame_below and
             pmm_free_frame; record the call chain of every live allocation
    report : show frames still allocated since tracking began, grouped by caller
    clear  : forget everything recorded so far
    off    : remove the breakpoints"""

    def __init__(self):
        super().__init__("frame-track", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        cmd = (gdb.string_to_argv(arg) or ["report"])[0]
        if cmd == "on":
            if _Track.bps:
                gdb.write("  already tracking\n")
                return
            for fn in ("pmm_alloc_frame", "pmm_alloc_frame_below"):
                _Track.bps.append(_AllocBP(fn, internal=True))
            _Track.bps.append(_FreeBP("*pmm_free_frame", internal=True))
            gdb.write("  tracking on (frames allocated earlier are not known)\n")
        elif cmd == "off":
            for b in _Track.bps:
                b.delete()
            _Track.bps = []
            gdb.write("  tracking off\n")
        elif cmd == "clear":
            _Track.live.clear()
            _Track.allocs = _Track.frees = _Track.untracked_frees = 0
            _Track.untracked.clear()
            gdb.write("  cleared\n")
        elif cmd == "report":
            self.report()
        else:
            raise gdb.GdbError("use: frame-track on|off|report|clear")

    def report(self):
        gdb.write(f"  allocs={_Track.allocs} frees={_Track.frees} "
                  f"live={len(_Track.live)} "
                  f"frees_of_untracked={_Track.untracked_frees}\n")
        groups = {}
        for pa, who in _Track.live.items():
            groups.setdefault(who, []).append(pa // FRAME_SIZE)
        for who, frames in sorted(groups.items(), key=lambda kv: -len(kv[1])):
            gdb.write(f"  {len(frames):>4} x {who}\n")
            gdb.write(f"         {_fmt_ranges(frames, 6)}\n")
        for pa, who in _Track.untracked:
            tag = "  (VIRTUAL address!)" if pa >= 0xFFFF800000000000 else ""
            gdb.write(f"  free of unknown 0x{pa:x}{tag} from {who}\n")


FrameTrack()


class PtWalk(gdb.Command):
    """pt-walk VA [CR3]: print the entry at each paging level for VA, reading
    the tables through the direct map. CR3 defaults to the current one."""
    DMAP = 0xFFFFFF8000000000
    MASK = 0x000FFFFFFFFFF000
    NAMES = ["PML4", "PDPT", "PD", "PT"]

    def __init__(self):
        super().__init__("pt-walk", gdb.COMMAND_DATA)

    @staticmethod
    def _flags(e):
        bits = (("P", 0), ("W", 1), ("U", 2), ("S", 7), ("NX", 63))
        return " ".join(n if e >> b & 1 else "-" for n, b in bits)

    def invoke(self, arg, from_tty):
        argv = gdb.string_to_argv(arg)
        if not argv:
            raise gdb.GdbError("use pt-walk VA [CR3]")
        va = int(gdb.parse_and_eval(argv[0])) & 0xFFFFFFFFFFFFFFFF
        cr3 = int(gdb.parse_and_eval(argv[1] if len(argv) > 1 else "$cr3"))
        table = cr3 & self.MASK
        mem = gdb.selected_inferior()
        for lvl in range(4):
            idx = (va >> (39 - 9 * lvl)) & 0x1FF
            raw = bytes(mem.read_memory(self.DMAP + table + idx * 8, 8))
            ent = int.from_bytes(raw, "little")
            gdb.write(f"  {self.NAMES[lvl]:<4}[{idx:>3}] @0x{table:x} = "
                      f"0x{ent:016x}  {self._flags(ent)}\n")
            if not ent & 1:
                gdb.write("    -> not present\n")
                return
            if lvl in (1, 2) and ent & 0x80:
                gdb.write("    -> large page, walk ends\n")
                return
            table = ent & self.MASK
        gdb.write(f"  phys 0x{table | (va & 0xFFF):x}\n")


PtWalk()


import struct


class PtTask(gdb.Command):
    """pt-task ID [-k]: list the mapped virtual ranges of a task's address
    space by walking its page tables. -k also walks the shared kernel half."""
    DMAP = 0xFFFFFF8000000000
    MASK = 0x000FFFFFFFFFF000

    def __init__(self):
        super().__init__("pt-task", gdb.COMMAND_DATA)

    def _table(self, phys):
        raw = gdb.selected_inferior().read_memory(self.DMAP + phys, 4096)
        return struct.unpack("<512Q", bytes(raw))

    def _leaves(self, phys, level, va, w, u, nx, limit):
        """Yield (va, size, w, u, nx) for every mapped page below this table.
        w/u are ANDed and nx is ORed down the walk, as the CPU does."""
        for idx, e in enumerate(self._table(phys)):
            if idx >= limit:
                break
            if not e & 1:
                continue
            v = va | idx << (39 - 9 * level)
            fw, fu, fnx = w & (e >> 1) & 1, u & (e >> 2) & 1, nx | (e >> 63)
            if level == 3 or (level > 0 and e & 0x80):
                if v >> 47 & 1:
                    v |= 0xFFFF << 48
                yield v, 1 << (39 - 9 * level), fw, fu, fnx
            else:
                yield from self._leaves(e & self.MASK, level + 1, v,
                                        fw, fu, fnx, 512)

    @staticmethod
    def _emit(r):
        s, e, w, u, nx = r
        perm = "r" + ("w" if w else "-") + ("-" if nx else "x")
        gdb.write(f"  0x{s:016x}-0x{e:016x}  {(e - s) >> 12:>6} pages  "
                  f"{perm} {'user' if u else 'kern'}\n")

    def invoke(self, arg, from_tty):
        argv = gdb.string_to_argv(arg)
        if not argv:
            raise gdb.GdbError("use pt-task ID [-k]")
        want = int(gdb.parse_and_eval(argv[0]))
        task = None
        for t in LxTasks._tasks():
            if int(t.dereference()["id"]) == want:
                task = t.dereference()
                break
        if task is None:
            raise gdb.GdbError(f"no task with id {want}")
        root = int(task["pml4_phys"]) & self.MASK
        limit = 512 if "-k" in argv[1:] else 256
        gdb.write(f"  task {want}: PML4 @0x{root:x}\n")
        run = None
        for v, size, w, u, nx in self._leaves(root, 0, 0, 1, 1, 0, limit):
            if run and run[1] == v and run[2:] == [w, u, nx]:
                run[1] = v + size
                continue
            if run:
                self._emit(run)
            run = [v, v + size, w, u, nx]
        if run:
            self._emit(run)
        else:
            gdb.write("  (no mappings)\n")


PtTask()
