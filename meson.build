project('Aegisub', ['c', 'cpp'], license : 'BSD-3-Clause',
        meson_version : '>=0.46.0',
        default_options : ['cpp_std=c++11', 'b_lto=true'])

subdir('build')

dataroot = join_paths(get_option('prefix'), get_option('datadir'), 'aegisub')
add_project_arguments('-DP_DATA="@0@"'.format(dataroot), language : 'cpp')
b_static = get_option('static')

conf = configuration_data()
conf.set_quoted('P_DATA', dataroot)
if get_option('build_credit') != ''
    conf.set_quoted('BUILD_CREDIT', get_option('build_credit'))
endif
conf.set('WITH_UPDATE_CHECKER', get_option('enable_update_checker'))

deps = []

if host_machine.system() == 'darwin'
    add_languages('objc')
else
    deps += dependency('fontconfig', static: b_static)
endif

cxx = meson.get_compiler('cpp')
cc = meson.get_compiler('c')
deps += cc.find_library('m', required : false)
deps += cc.find_library('dl', required : false)
deps += cc.find_library('iconv', required : false)

deps += dependency('libass', version : '>=0.9.7', static: b_static)
deps += dependency('boost', version : '>=1.50.0', static: b_static,
                   modules : ['chrono', 'filesystem', 'locale', 'regex',
                              'system', 'thread'])
deps += dependency('icu-uc', version : '>=4.8.1.1', static: b_static)
deps += dependency('icu-i18n', version : '>=4.8.1.1', static: b_static)
deps += dependency('zlib', static: b_static)

dep_avail = []
foreach dep : [
    # audio, in order of precedence
    ['libpulse', '', 'PulseAudio'],
    ['alsa', '', 'ALSA'],
    ['portaudio-2.0', '', 'PortAudio'],
    ['openal', '>=0.0.8', 'OpenAL'],
    # video
    ['ffms2', '', 'FFMS2'],
    # other
    ['fftw3', '', 'FFTW3'],
    ['hunspell', '', 'Hunspell'],
    ['uchardet', '', 'uchardet'],
]
    d = dependency(dep[0], version : dep[1] != '' ? dep[1] : '>=0',
                   required : false, static: b_static)

    optname = 'enable_@0@'.format(dep[0].split('-')[0])
    if d.found() and get_option(optname) != 'false'
        deps += d
        conf.set('WITH_@0@'.format(dep[0].split('-')[0].to_upper()), '1')
        dep_avail += dep[2]
    elif get_option(optname) == 'true'
        error('@0@ enabled but not found'.format(dep[2]))
    endif
endforeach

# TODO: OSS

def_audio = get_option('default_audio_output')
if def_audio != 'auto'
    if not dep_avail.contains(def_audio)
        error('Default audio output "@0@" selected but not available'.format(def_audio))
    endif
elif dep_avail.length() != 0
    def_audio = dep_avail[0]
else
    def_audio = ''
endif

conf_platform = configuration_data()
conf_platform.set('DEFAULT_PLAYER_AUDIO', def_audio)

luajit = dependency('luajit', version : '>=2.0.0', required : get_option('system_luajit'))
if luajit.found()
    luajit_test = cc.run('''#include <lauxlib.h>
int main(void)
{
    lua_State *L = luaL_newstate();
    if (!L) return 1;
    // This is valid in lua 5.2, but a syntax error in 5.1
    const char testprogram[] = "function foo() while true do break return end end";
    return luaL_loadstring(L, testprogram) == LUA_ERRSYNTAX;
}''', dependencies : luajit)

    if luajit_test.returncode() == 1
        if get_option('system_luajit')
            error('System luajit found but not compiled in 5.2 mode')
        else
            message('System luajit found but not compiled in 5.2 mode; using built-in luajit')
        endif
    else
        deps += luajit
    endif
else
    message('System luajit not found; using built-in luajit')
endif

if not deps.contains(luajit)
    luajit_sp = subproject('luajit')
    luajit_inc = luajit_sp.get_variable('incdir')
    deps += luajit_sp.get_variable('luajit_dep')
else
    luajit_inc = include_directories(luajit.get_pkgconfig_variable('includedir'))
endif
subdir('vendor/luabins/src')

deps += dependency('wxWidgets', version : '>=3.0.0',
                   modules : ['std', 'stc', 'gl'])

dep_gl = dependency('gl', required : false)
if not dep_gl.found()
    if host_machine.system() == 'windows'
        dep_gl = cc.find_library('opengl32', required : false)
    else
        dep_gl = cc.find_library('GL', required : false)
    endif

    if not cc.has_header('GL/gl.h')
        dep_gl = dependency('', required : false)
    endif
endif

if not dep_gl.found()
    error('OpenGL implementation not found')
endif

deps += dep_gl

acconf = configure_file(output : 'acconf.h', configuration : conf)

subdir('automation')
subdir('libaegisub')
subdir('packages')
subdir('po')
subdir('src')
