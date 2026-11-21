import gdb

live = {}
stats = {}
seq = [0]

PLUMBING = {
    "kcache_alloc",
    "kcache_zalloc",
    "kcache_create",
    "kmalloc",
    "kzalloc",
    "kfree",
    "kcache_free",
}

MASK64 = 0xFFFFFFFFFFFFFFFF


def real_caller():
    f = gdb.newest_frame()
    while f is not None:
        name = f.name()
        if name and name not in PLUMBING:
            return name
        f = f.older()
    return "?"


class AllocReturn(gdb.FinishBreakpoint):
    def __init__(self, frame, cache, size, who):
        super().__init__(frame, internal=True)
        self.cache, self.size, self.who = cache, size, who

    def stop(self):
        rv = self.return_value
        if rv is None:
            rv = gdb.parse_and_eval("$rax")
        addr = int(rv) & MASK64
        if addr:
            seq[0] += 1
            live[addr] = dict(
                cache=self.cache, size=self.size, who=self.who, seq=seq[0]
            )
            stats.setdefault(self.cache, [0, 0])[0] += 1
            gdb.write(
                "alloc 0x%x cache=%s objsize=%d by %s\n"
                % (addr, self.cache, self.size, self.who)
            )
        return False

    def out_of_scope(self) -> None:
        pass


class AllocEntry(gdb.Breakpoint):
    def __init__(self):
        super().__init__("*kcache_alloc", internal=True)

    def stop(self):
        try:
            c = gdb.parse_and_eval("(struct kcache *)$rdi")
            AllocReturn(
                gdb.newest_frame(), c["name"].string(), int(c["objsize"]), real_caller()
            )
        except gdb.error as e:
            gdb.write("kmem_track: alloc hook failed: %s\n" % e)
        return False


class FreeEntry(gdb.Breakpoint):
    def __init__(self):
        super().__init__("*kcache_free", internal=True)
    def stop(self):
        addr = int(gdb.parse_and_eval("$rdi")) & MASK64
        if addr == 0:
            return False
        info = live.pop(addr, None)
        if info is None:
            gdb.write("free 0x%x UNKNOWN (double free, or allocated before " 
                      "tracking record) by %s\n" % (addr, real_caller()))
        else:
            stats.setdefault(info["cache"], [0, 0])[1] += 1
            gdb.write("free  0x%x  cache=%s  by %s\n" %
                      (addr, info["cache"], real_caller()))
        return False

class KmemLive(gdb.Command):
	def __init__(self):
		super().__init__("kmem-live", gdb.COMMAND_DATA)
	def invoke(self, arg, tty):
		if not live:
			gdb.write("no live allocations\n")
			return
		gdb.write("%-18s %-14s %7s  %s\n" % ("ADDR", "CACHE", "OBJSIZE", "ALLOCATED BY"))
		for addr, i in sorted(live.items(), key=lambda kv: kv[1]["seq"]):
			gdb.write("0x%-16x %-14s %7d  %s\n" % (addr, i["cache"], i["size"], i["who"]))

class KmemStats(gdb.Command):
    def __init__(self):
        super().__init__("kmem-stats", gdb.COMMAND_DATA)
    def invoke(self, arg, from_tty):
        gdb.write("%-14s %7s %7s %7s\n" % ("CACHE", "ALLOCS", "FREES", "LIVE"))
        for name, (a, f) in sorted(stats.items()):
            gdb.write("%-14s %7d %7d %7d\n" % (name, a, f, a - f))

class KmemWhere(gdb.Command):
	def __init__(self):
		super().__init__("kmem-where", gdb.COMMAND_DATA)
	def invoke(self, arg, tty):
		a = int(gdb.parse_and_eval(arg)) & MASK64
		for base, i in live.items():
			if base <= a < base + i["size"]:
				gdb.write("0x%x is in object 0x%x (+%d) of cache %s" % (a, base, a-base, i["cache"], i["who"]))
				return
		gdb.write("0x%x is not inside any tracked live object\n" % a)

class KmemClear(gdb.Command):
	def __init__(self):
		super().__init__("kmem-clear", gdb.COMMAND_DATA)
	def invoke(self, arg, tty):
		live.clear()
		stats.clear()

AllocEntry()
FreeEntry()
KmemLive()
KmemStats()
KmemWhere()
KmemClear()
gdb.write("kmem_track loaded: hooking kcache_alloc / kcache_free\n")
