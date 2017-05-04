import sys
from pyew import CPyew
from shutil import copyfile


def main(f):

    pyew = CPyew(plugins=True, batch=True)
    pyew.loadFile(f)
    print "[*] Found %d bbs" % len(pyew.basic_blocks)
    bblDict = dict.fromkeys(pyew.basic_blocks)
    print "[*] Now writing changed version ... "
    copyfile(f, "patched_" + f)

    fh = open("patched_" + f, "r+")
    for bb in pyew.basic_blocks:

        print hex(bb)
        fh.seek(bb)
        bblDict[bb] = fh.read(1)

        fh.seek(0)
        fh.seek(bb)
        fh.write("\xcc")

    fh.close()

    descr = open("patched_description", "w")
    for bbl in bblDict.keys():
        descr.write(str(bbl))
        descr.write(":")
        descr.write(str(ord(bblDict[bbl])))
        descr.write("\n")

    descr.close()
    print "[+] bye bye"


if __name__ == "__main__":
    if len(sys.argv) == 1:
        print "Usage:", sys.argv[0], "<program file>"
    else:
        main(sys.argv[1])
