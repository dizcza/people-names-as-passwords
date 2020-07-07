# People names as passwords

Parse popular wordlists to create masks of how people names are used as passwords. These masks can be directly fed into [hashcat](https://github.com/hashcat/hashcat).

The outcome of this research project will be eventually utilized in [hashcat-wpa-server](https://github.com/dizcza/hashcat-wpa-server) as an additional attack vector.

## Quick run

```
$ sudo apt install konwert dos2unix
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

#### Limitations:

* all original words containing symbol `|` are excluded from the wordlist;
* only names of at least 4 characters long are considered to collect the statistics. You can change this option in [load_valid_names.sh](bash/load_valid_names.sh). Later on, at passwords generation phase, names of all lengths will be generated.


## In Depth

### Data source

50k first names from 54 countries, collected by a German coomputer magazine under GNU Free Documentation License in 2002, are mirrored onto [Dropbox](https://www.dropbox.com/s/l0mskgdp1hsv04n/0717-182.zip) to prevent flooding the original source.

The first names are parsed into ASCII equivalent words and stored in `gender/names` folder (see [load_valid_names.sh](bash/load_valid_names.sh)).


### Most used first names

Most used people first names of length 5 and more:

```
$ awk '{if (length($2) >= 5) print}' masks/most_used_names.sorted | head
139989 chris
128067 angel
104553 ester
100876 cally
 99607 inger
 94240 andre
 91455 ander
 86502 erman
 86332 erden
 77870 christ
```


### Creating masks

[OMEN](https://github.com/RUB-SysSec/OMEN) was used to create the alphabet of Top2B passwords:

```
eainr7s0o1lt3542986ducmhgbkpyvfwAzEjIRSNCDxOBTLGHqMFKPUYWJVZXQ-'.!$@+_?#/=:)("~&,%{*`\^}>;[<]|
```

The last ASCII symbol is `|`, meaning it's least used in password candidates. The probability of encountering `|` in a wordlist is `0.008 %`. Therefore, we can use this symbol as a mask after we strip all lines from the wordlist that contain `|`, which results in preserving approx. `99.9 %` of words.

The [implementation](src/create_masks.py) employs an efficient and simple data structure - a trie (a prefix tree) - that has proven to yield the lowest algorithmic complexity of searching substrings in a text when the look-up name strings are drawn from a small alphabet (in our case it's 26 English lowercase characters). The search is case insensitive.

### Size: 'wpa' and 'all'

* `wpa`: at least 8 characters long passwords
* `all`: all passwords, no filtering by length


### Mode: 'length' and 'single'

#### Length Mode

Each name in a password is replaced by `|`, keeping the length:

* `12vika1992 --> 12||||1992`
* `Gunter@! --> ||||||@!`

That's where hashcat masks come into place. To be simple, only
* lowercase all
* uppercase all
* capitalize

masked rules are created for each match. That is, `12vika1992` creates 3 masks, valid for WPA hashcat attack:

* `12l?l?l?l?1992`
* `12u?u?u?u?1992`
* `12u?l?l?l?1992`

Masks in `masks.hashcat` files are sorted by the num. of occurrences, taken from `masks.stats`.

#### Single Char Mode

In the single mode, each match group is replaced with a single symbol `|`:

* `12vika1992 --> 12|1992`
* `Gunter@! --> |@!`

While not suited directly for hashcat masks, the placeholder `|` serves  for direct substitution of names of arbitrary length (see next chapter). Examples from Top29M wordlist:

```
$ head -n10 masks/all/single/masks.stats
 242364 |
  44311 |1
  33654 |123
  33307 |s
  31914 |a
  29447 |e
  26605 |2
  25756 |12
  23953 |n
  23168 |3
```

Here is how you read single char mode output:

* plain names of any length were used 242364 times as passwords;
* digit `1` was appended to names of any length 44311 times ...


## Generate probable passwords

Brute forcing all lower cases in `l?l?l?l?l?l?l?1` x 1000 to look for name patterns might be not the best idea. A smarter way is to substitute each `|` in a _single_ mode and each group of subsequent `|` characters in _length_ mode by a name from a chosen country. Doing so will dramatically decrease the search space.

Examples:

```
single mode: 12|1992  --> 12jan1992 12alex1992 12simon1992 12martina1992 12michael1992 ...
length mode: 12||||1992  -->  12alex1992 12lena1992 12lora1992 12vika1992 12sofi1992 ...
```

(to be continued)
