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

printf '[127:64] (W3,W2)      dsc_base    0x%08x%08x\n' $w3 $w2

printf '\nW1 0x%08x:\n' $w1

let "v = ($w1 >> 29) & 0x7F"
printf '[63:61]  (W1[31:29])  rsv         0x%x\n' $v

let "v = ($w1 >> 28) & 0x1"
printf '[60]     (W1[28])     err_wb_sent 0x%x\n' $v

let "v = ($w1 >> 26) & 0x3"
printf '[59:58]  (W1[27:26]   err         0x%x\n' $v

let "v = ($w1 >> 25) & 0x1"
printf '[57]     (W1[25])     irq_no_last 0x%x\n' $v

let "v = ($w1 >> 24) & 0x1"
printf '[56]     (W1[24])     irq_pnd     0x%x\n' $v

let "v = ($w1 >> 22) & 0x3"
printf '[55:54]  (W1[23:22]   rsv0        0x%x\n' $v

let "v = ($w1 >> 21) & 0x1"
printf '[53]     (W1[21])     irq_en      0x%x\n' $v

let "v = ($w1 >> 20) & 0x1"
printf '[52]     (W1[20])     wbk_en      0x%x\n' $v

let "v = ($w1 >> 19) & 0x1"
printf '[51]     (W1[19])     mm_chn      0x%x\n' $v

let "v = ($w1 >> 18) & 0x1"
printf '[50]     (W1[18])     byp         0x%x\n' $v

let "v = ($w1 >> 16) & 0x3"
printf '[49:48]  (W1[17:16]   dsc_sz      0x%x\n' $v

let "v = ($w1 >> 12) & 0xF"
printf '[47:44]  (W1[15:12]   rng_sz      0x%x\n' $v

let "v = ($w1 >> 4) & 0xFF"
printf '[43:36]  (W1[11:4]    fnc_id      0x%x\n' $v

let "v = ($w1 >> 3) & 0x1"
printf '[35]     (W1[3])      wbi_acc_en  0x%x\n' $v

let "v = ($w1 >> 2) & 0x1"
printf '[34]     (W1[2])      wbi_chk     0x%x\n' $v

let "v = ($w1 >> 1) & 0x1"
printf '[33]     (W1[1])      fcrd_en     0x%x\n' $v

let "v = $w1 & 0x1"
printf '[32]     (W1[0])      qen         0x%x\n' $v

printf '\nW0 0x%08x:\n' $w0

let "v = ($w0 >> 17) & 0x7FFF"
printf '[31:17]  (W0[31:17]   rsv         0x%x\n' $v

let "v = ($w0 >> 16) & 0x1"
printf '[16]     (W0[16])     irq_ack     0x%x\n' $v

let "v = $w0 & 0xFF"
printf '[15:0]   (W0[15:0]    pidx        0x%x\n' $v
