# this is run pre-build, add to platformio.ini like "extra_scripts = pre:env.py"
Import("env")
#env.ProcessUnFlags("BLYNK_TOKEN")
#env.Append(CPPDEFINES=[ ("BLYNK_TOKEN", "YvC4EfhIoGyWjgxEumhj-txdtwooSyg4") ])