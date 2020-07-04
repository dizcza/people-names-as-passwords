# Credits: https://unix.stackexchange.com/a/596125/183393
# usage: awk -f create_masks.awk /path/to/names /path/to/wordlist
# The below assumes that no line in /path/to/names is longer than 512 characters.

BEGIN {
    dots = sprintf("%*s",512,"")
    gsub(/ /,"|",dots)
    resLength = "masks/masks.raw"
}
{ lc = tolower($0) }
NR==FNR {
    lgth = length($0)
    str2lgth[lc] = lgth
    str2dots[lc] = substr(dots,1,lgth)
    next
}
{
    for (str in str2lgth) {
        if ( s=index(lc,str) ) {
            bef = substr($0,1,s-1)
            aft = substr($0,s+str2lgth[str])
            print bef str2dots[str] aft > resLength
        }
    }
}