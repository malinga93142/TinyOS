import gdb

class SlabFinishBreakPoint(gdb.FinishBreakpoint):
	def __init__(self, frame):
		super().__init__(frame, internal=True)
	def stop(self):
		try:
			s = self.return_value
			if not s or int(s) == 0:
				return False
			c = s['cache']
			objsize= int(s['objsize'])
			objperslab=int(s['obj_per_slab'])
			freeptr = s['free']
			print(f"slab access: {s}\n")
		except Exception as e:
			print(f"{e}")
		return False

class SlabNewBreakPoint(gdb.Breakpoint):
	def __init__(self):
		super().__init__("slab_new", gdb.BP_BREAKPOINT, internal=False)
	def stop(self):
		SlabFinishBreakPoint(gdb.selected_frame().older())
		return False

SlabNewBreakPoint()
print("slab inspection script loaded\n")
