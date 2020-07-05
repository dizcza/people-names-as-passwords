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
* a plain name of exactly 8 characters appeared 4629 times;
* a name of length 7 with suffix `1` appeared 2005 times ...

The `run.sh` script needs to be run only once. Hashcat masks (not shown here) will be stored in `masks/**/length/masks.hashcat`.

#### Limitations:

* \[pre-processing\] all original words containing symbol `|` are excluded from a wordlist;
* \[pre-processing\] only names of at least 4 characters long are considered (you can change this option in [load_valid_names.sh](bash/load_valid_names.sh));
* \[post-processing\] only top 1000 masks are retrieved from `masks.raw` (you can change this option in [masks_statistics.sh](bash/masks_statistics.sh)).


## In Depth

### Data source

50k first names from 54 countries, collected by a German coomputer magazine under GNU Free Documentation License in 2002, are mirrored onto [Dropbox](https://www.dropbox.com/s/l0mskgdp1hsv04n/0717-182.zip) to prevent flooding the original source.

The first names are parsed into ASCII equivalent words and stored in `gender/names` folder (see [load_valid_names.sh](bash/load_valid_names.sh)).

### Creating masks

[OMEN](https://github.com/RUB-SysSec/OMEN) was used to create an alphabet of Top2B passwords:

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
$ head -n10 masks/wpa/single/masks.stats
    836 iloveyou|
    834 |123456789
    725 princess|
    676 michael|
    667 |johnson
    660 |williams
    650 |1234567
    616 jessica|
    614 |michael
    609 |12345678
```

Here is how you read single char mode output:

* a name of any length was added to `iloveyou` 836 times;
* a name of any length was a prefix of `123456789` 834 times ...


## "Smart" masks

Brute forcing all lower cases in `12l?l?l?l?1992` x 1000 to look for name patterns might be not the best idea. A smarter way is to substitute each `|` in a _single_ mode and each group of `|` x N in _length_ mode by a corresponding name from a chosen country. Doing so will dramatically decrease the search space.

(to be continued)
