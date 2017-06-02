# coverager
Tool to determine code coverage for closed source applications in linux. 

This tool determines the basic blocks of a binary that were hit during execution. 
For this the first byte of each basic block is overwritten with a CC byte, which causes a SIGTRAP/break signal. 
Coverager catches the signals (using ptrace) and maps the instruction pointer to the original first byte of the basic block.
The CC byte is then overwritten with the original byte and the instruction pointer decremented by one.

Coverager outputs the amount of basic blocks that were hit and also generates a bitmap file, to determine which basic blocks 
were hit. 

For generating the patched binary I use a python script that uses the hopper api to determine the basic blocks.

## Usage:

	./coverager <call_string> <parameter> <patched_description> <output> <path_to_patched_object> <timeout>

## Example workflow for evince (pdf viewer):

### Generating patched binary and description:

	Load the binary in hopper and use the 'exportBBs.py' script. This generates the patched binary and a description file that 
	maps the offset of a CC byte to the original byte in the form of: 'offset:original byte'. Both in dec.

### Overwrite the original library with the patched binary

	chmod +x patched_binary && sudo cp patched_binary /usr/lib/libevview.so.3.0.0
	
### Call coverager
	
	./coverager "/usr/bin/evince" "pdf.pdf" patched_description bbs_map /usr/lib/libevview.so.3.0.0 1
	
### Output

	[*] Coverager
	[*] Parsing description file
	[+] Parsing Done!
	[+] Found 7687 bbs
	[+] Starting monitoring pid: 21675
	[*] about to call execve ...
	[+] /usr/lib/libevview3.so.3.0.0 is at 0x7f8c3fb8d000
	[+] new thread detected, t_id: 21677
	[+] new thread detected, t_id: 21678
	[+] new thread detected, t_id: 21679
	[+] new thread detected, t_id: 21683
	[+] new thread detected, t_id: 21684
	[+] new thread detected, t_id: 21685
	[+] new thread detected, t_id: 21686
	[+] new thread detected, t_id: 21687
	[!] Child was killed by timeout
	[!] BBS hit: 1391

You will also find a bitmap file in the directory.
In case of a sigsev coverager will print "[!] sigsev" to stderr
	

	
