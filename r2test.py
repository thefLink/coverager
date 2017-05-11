import r2pipe 
import json
import sys

from shutil import copyfile

def main(f):

    copyfile(f, "patched_" + f)

    r2 = r2pipe.open(f)
    r2.cmd("aa")

    # Check for offset to subtract in case of 
    # and entrypoint
    entrypoint_json = json.loads(r2.cmd("iej"))[0]
    offset = entrypoint_json['baddr']

    bbs_addresses = []
    all_functions = json.loads(r2.cmd("aflj"))
    first = True
   
    print "[*] Found functions: %d" % len(all_functions)
    cntr = 0
    for function in all_functions[0:]:
        cntr += 1

        if cntr % 100 == 0:
            print "[*] analysing func nr: %d/%d" % (cntr, len(all_functions))

        try:
            bbs = json.loads(r2.cmd('afbj ' + function['name']))
            for bb in bbs:
                if bb['addr'] - offset > 1000:
                    if (bb['addr'] - offset) not in bbs_addresses:
                       bbs_addresses.append(bb['addr'] - offset)
        except:
            pass



    print len(bbs_addresses)

    bbs_map = dict.fromkeys(bbs_addresses)
    fh = open("patched_" + f, "r+")

    for bb in bbs_addresses:
        fh.seek(bb)
        bbs_map[bb] = fh.read(1)

        fh.seek(0)
        fh.seek(bb)
        fh.write("\xcc")

    fh.close()

    descr_file = open("patched_description", "w")
    for bbl in bbs_map.keys():
        descr_file.write(str(bbl))
        descr_file.write(":")
        descr_file.write(str(ord(bbs_map[bbl])))
        descr_file.write("\n")

    descr_file.close()
    print "[+] cool, bye"
            
if __name__ == "__main__":
    if len(sys.argv) == 1:
        print "Usage:", sys.argv[0], "<program file>"
    else:
        main(sys.argv[1])
