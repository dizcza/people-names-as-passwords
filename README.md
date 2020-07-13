# People names as passwords

Parse popular wordlists to create masks of how people names are used as passwords. These masks can be directly fed into [hashcat](https://github.com/hashcat/hashcat).

The outcome of this research project will be eventually utilized in [hashcat-wpa-server](https://github.com/dizcza/hashcat-wpa-server) as an additional attack vector.

## Quick run

### Create masks

```
$ sudo apt install konwert
$ ./run.sh
$ head -n10 masks/wpa/length/masks.stats
   4629 ||||||||
   2005 |||||||1
   1878 |||||123
   1804 |||||||||
   1103 ||||||123
   1068 |||||||s
    963 ||||||12
    795 ||||||||1
    749 ||||||01
    716 ||||1234
```

(You will see different output, depending on the choice of a wordlist to scan for names.)

Here is how you read the output:
* plain names of exactly 8 characters appeared 4629 times;
* names of length 7 with suffix `1` appeared 2005 times ...

The `run.sh` script needs to be run only once. Hashcat masks (not shown here) will be stored in `masks/**/length/masks.hashcat`.

### Generate probable passwords

Based on the masks statistics from the previous step, we can generate new passwords:

```
gcc -O1 -o generate src/generate.c
./generate -m 1000 gender/names/ascii.valid masks/masks.stats
```

The script above takes top `1000` masks from `masks/masks.stats`, assuming you already did `./run.sh`, and prints new password candidates to the standard output. The output can be piped in `hashcat --stdin`. Password generation is fast - approximately 14M candidates per second.


## In Depth

* [create masks from names explained](doc/create_masks.md)
* [generate new passwords explained](doc/generate.md)
