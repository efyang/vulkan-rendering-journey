glsllang = find_program('glslangValidator')
shaders = [
	'basic_flat_mesh.frag',
	'basic_normalcolor_mesh.vert',
	'basic_vertexcolor_mesh.vert',
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