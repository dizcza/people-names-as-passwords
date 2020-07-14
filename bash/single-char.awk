# sum the counts of the subsequent mask lines
BEGIN {
    getline;
    count=$1;
    mask=$2;
}
{
    if ($2 == mask) {
        count += $1;
    } else {
        print count, mask;
        count = $1;
        mask = $2;
    }
}
END {
    print count, mask;
}
