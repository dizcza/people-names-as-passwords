## Generate probable passwords

Brute forcing all lower cases in `l?l?l?l?l?l?l?1` x 1000 to look for name patterns might be not the best idea. A smarter way is to substitute each `|` in a _single_ mode and each group of subsequent `|` characters in _length_ mode by a name from a chosen country. Doing so will dramatically decrease the search space.

### Examples

```
single mode: 12|1992  --> 12jan1992 12alex1992 12simon1992 12martina1992 12michael1992 ...
length mode: 12||||1992  -->  12alex1992 12lena1992 12lora1992 12vika1992 12sofi1992 ...
```

### Usage

Refer to [README](../README.md).
