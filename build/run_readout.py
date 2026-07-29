import subprocess as sp

result = sp.run(["ps", "aux"], capture_output=True, text=True, check=True)

if 'GramsReadout' in result.stdout:
    print('GramsReadout already running! Aborting...')
    exit()

else:
    sp.run("sudo ./GramsReadout", shell=True)