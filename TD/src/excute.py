"""
Execute DAT

me - this DAT

Make sure the corresponding toggle is enabled in the Execute DAT.
"""

def onStart():
	"""
	Called when the project starts.
	"""
	op('mode').par.Value0 = 1
	op('mode1').par.Value0 = 1
	op('mode2').par.Value0 = 1
	op('timeline').time.frame = 0
	op('timeline').time.play = 0
	op('../perform').par.performance.pulse()
	return

def onCreate():
	"""
	Called when the DAT is created.
	"""
	return

def onExit():
	"""
	Called when the project exits.
	"""
	op('mode').par.Value0 = 1
	op('mode1').par.Value0 = 1
	op('mode2').par.Value0 = 1
	return

def onFrameStart(frame: int):
	"""
	Called at the start of each frame.
	
	Args:
		frame: The current frame number
	"""
	return

def onFrameEnd(frame: int):
	"""
	Called at the end of each frame.
	
	Args:
		frame: The current frame number
	"""
	return

def onPlayStateChange(state: bool):
	"""
	Called when the play state changes.
	
	Args:
		state: False if the timeline was just paused
	"""
	return

def onDeviceChange():
	"""
	Called when a device change occurs.
	"""
	return

def onProjectPreSave():
	"""
	Called before the project is saved.
	"""
	return

def onProjectPostSave():
	"""
	Called after the project is saved.
	"""
	return
