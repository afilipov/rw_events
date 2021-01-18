ev_common_src = files(
    'src/common.c'
)

ev_common_inc = [
    'inc'
]

ev_args = [
	'-Wmissing-parameter-type',
	'-Wold-style-declaration',
	'-Wimplicit-int',
	'-Wimplicit-function-declaration',
	'-Wimplicit',
	'-Wignored-qualifiers',
	'-Wformat-nonliteral',
	'-Wcast-align',
	'-Wpointer-arith',
	'-Wbad-function-cast',
	'-Wmissing-prototypes',
	'-Wstrict-prototypes',
	'-Wmissing-declarations',
	'-Wnested-externs',
	'-Wshadow',
	'-Wwrite-strings',
	'-Wfloat-equal',
	'-Woverride-init',
	'-fno-builtin',
        '-D_DEFAULT_SOURCE'
]

ev_common = shared_library('rwcommon',
                               ev_common_src,
                               include_directories: ev_common_inc,
                               c_args: ev_args,
                               install: true,
                               install_dir: lib_dir
                               )

ev_dependencies = [
    ev_common,
]

ev_record_src = files(
    'run/record.c'
)

executable('ev_record',
           ev_record_src,
           include_directories: ev_common_inc,
           c_args: ev_args,
           link_with: ev_dependencies,
           install: true)

ev_replay_src = files(
    'run/replay.c'
)

executable('ev_replay',
           ev_replay_src,
           include_directories: ev_common_inc,
           c_args: ev_args,
           link_with: ev_dependencies,
           install: true)
