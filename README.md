# coverager
Generating code coverage for closed source applications [linux]

This is a port of Jaanus Kääp's 'Kaval Ants' to linux.

https://github.com/JaanusFuzzing/KavalAnts

The basic idea is to overwrite the first byte of each basic block with a CC byte which causes an INT3 event during execution.
When an int3 event occurs the CC is overwritten with the original byte and execution is continued. 

Using this approach code coverage can be calculated extremly fast. I produce a bitmap for the basic blocks sothat we can distinguish
basic blocks that were hit during exectution with different corpuses.

I use the hopper script that is shipped with coverager to generate the patched binaries. 

```
./coverager
usage: coverager [-r run_path] [-p path to patched binary] [-m path to mapping file] [-t timeout] [-a parameters] [[-c prints the number of bbs hit]]
```

Example:
In this example I patched /usr/lib/libevview3.so.3.0.0 which is a lib of evince.

```
flink@Bigmachine coverager % ./coverager -r /usr/bin/evince -p /usr/lib/libevview3.so.3.0.0 -m /tmp/patched_description -t 1 -a /home/flink/Documents/French.pdf -c
1157
```

If you want the bitmap to be printed to stdout, start without -c
