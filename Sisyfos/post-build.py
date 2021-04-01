# this is run post-build, add to platformio.ini like "extra_scripts = post:post-build.py"
#Import("env")

# copy firmware to root folder
#env.Execute("cp -f $BUILD_DIR/firmware.bin " + env['PROJECT_DIR'] + "/");



Import("env")
import shutil
import os
#
# Dump build environment (for debug)
#print(env.Dump())
#

#
# Upload actions
#

firmware_source = os.path.join(env.subst("$BUILD_DIR"), "firmware.hex")
def after_build(source, target, env):
    #print(firmware_source)
    #env.Execute("cp -f $BUILD_DIR/firmware.bin " + env['PROJECT_DIR'] + "/")
    env.Execute("test -d release/ || mkdir release")
    shutil.copy(firmware_source, 'release/Sisyfos_latest.hex')    
    
env.AddPostAction("buildprog", after_build)
