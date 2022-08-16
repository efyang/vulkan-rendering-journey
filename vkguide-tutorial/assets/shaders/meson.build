glsllang = find_program('glslangValidator')
shaders = [
	'triangle.vert',
	'triangle.frag',
]  # full path with .glsl extension (or from subdir with files() extension)

foreach s : shaders
  shaders += custom_target(
    'shader @0@'.format(s),
    command : [glsllang, '-V', '@INPUT@', '-o',  '@OUTPUT@'],
    input : s, 
    output : '@PLAINNAME@.spv',
	build_by_default: true,
  )
endforeach