#!/bin/bash

if [ $# -lt 4 ]; then
	echo "$0: <w3> <w2> <w1> <w0>"
	exit
fi

w3=$1
w2=$2
w1=$3
w0=$4

let "w0 = $w0 + 0"
let "w1 = $w1 + 0"
let "w2 = $w2 + 0"
let "w3 = $w3 + 0"

printf '\n0x%08x 0x%08x 0x%08x 0x%08x\n\n' $w3 $w2 $w1 $w0

let "v = ($w3 >> 24) & 0x1"
printf '[120]     (W3[24])                      valid        0x%x\n' $v

let "v = ($w3 >> 8) & 0xFFFF"
printf '[119:104] (W3[23:8])                    cidx         0x%x\n' $v

let "v = $w3 & 0xFF"
let "v1 = ($w2 >> 24) & 0xFF"
let "v2 = ($v << 8) | $v1"
printf '[103:88]  (W3[7:0] W2[31:24])           pidx         0x%x\n' $v2

let "v = ($w2 >> 28) & 0x3"
printf '[87:86]   (W2[23:22])                   desc_size    0x%x\n' $v

let "v2 = $w2 & 0x3FFFFF"
let "v1 = ($w0 >> 28) & 0xF"
printf '[85:28]   (W2[21:0] W1[31:0] W0[31:28]) baddr_64     '
let "v2 = ($v2 << 4) | ($w1 >> 28)"
printf '0x%x ' $v2
let "v1 = ($w1 << 4) | $v1"
printf '%08x ' $v1

let "v = $v2 << 6 | ($v1 & 0x3F) >> 26"
let "v1 = $v1 << 6"
printf ' => 0x%x %08x\n' $v $v1


let "v = ($w0 >> 24) & 0xF"
printf '[27:24]   (W0[27:24])                   rng_sz       0x%x\n' $v

let "v = ($w0 >> 23) & 0x1"
printf '[23]      (W0[23])                      color        0x%x\n' $v

let "v = ($w0 >> 21) & 0x3"
printf '[22:21]   (W0[22:21])                   int_st       0x%x\n' $v

let "v = ($w0 >> 17) & 0xF"
printf '[20:17]   (W0[20:17])                   timer_idx    0x%x\n' $v

let "v = ($w0 >> 13) & 0xF"
printf '[16:13]   (W0[16:13])                   counter_idx  0x%x\n' $v

let "v = ($w0 >> 5) & 0xF"
printf '[12:5]    (W0[12:5])                    fnc_id       0x%x\n' $v

let "v = ($w0 >> 2) & 0x7"
printf '[4:2]     (W0[4:2])                     trig_mode    0x%x\n' $v

let "v = ($w0 >> 1) & 0x1"
printf '[1]       (W0[1])                       en_int       0x%x\n' $v

let "v = $w0 & 0x1"
printf '[0]       (W0[0])                       en_stat_desc 0x%x\n' $v
