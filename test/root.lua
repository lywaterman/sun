function dofile(filename)
	local f = assert(loadfile(filename))

	return f() 
end	
lanes = dofile('/home/liuyang/sun/sun/c_src/lanes/lanes.lua').configure({protect_allocator=true})


return


