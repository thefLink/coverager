from shutil import copyfile 

doc = Document.getCurrentDocument()
segments = doc.getSegmentsList()
f_path = doc.getExecutableFilePath()
bbs = []

print "==============================="
print "[+] Path: %s" % f_path

# Create bb list
for seg in segments:
    print seg.getName()
    for i in range(0, seg.getProcedureCount()):
        func = seg.getProcedureAtIndex(i)
        for j in range(0, func.getBasicBlockCount()):
            bb = func.getBasicBlock(j)
            bbs.append(bb.getStartingAddress() - seg.getStartingAddress())

print "[+] Found %d bbs" % len(bbs)
copyfile(f_path, "/tmp/patched_binary")
print "[!] Wrote copy to: /tmp/patched_binary"

# Create bb map
bb_map = dict.fromkeys(bbs)
fh = open("/tmp/patched_binary", "r+")

for bb in bbs:
    fh.seek(bb)
    bb_map[bb] = ord(fh.read(1))

    fh.seek(0)
    fh.seek(bb)
    fh.write("\xcc")

fh.close()

descr_file = open("/tmp/patched_description", "w")
for bb in bb_map.keys():
    descr_file.write(str(bb))
    descr_file.write(":")
    descr_file.write(str((bb_map[bb])))
    descr_file.write("\n")

descr_file.close()
print "[+] cool, bye"
