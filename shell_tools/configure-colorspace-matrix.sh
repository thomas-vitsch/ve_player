[A#!/bin/sh

##
# Copyright (c) 2017,
#      Daan Vreeken <Daan @ VEHosting . nl> - http://VEHosting.nl/
#      All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS `AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
# INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

# Zie helemaal onderaan deze file de ene call die het configureren in gang zet..




# Y in X parameter = max negatief= 0x1fff .. 0 .. 0x0fff	(max=4095) mag=2^10=1024
# U in X parameter = max negatief= 0x1fff .. 0 .. 0x0fff	(max=4095) mag=2^10=1024
# V in X parameter = max negatief= 0x1fff .. 0 .. 0x0fff	(max=4095) mag=2^10=1024
# const  parameter = max negatief= 0x3fff .. 0 .. 0x1fff	(max=8191) mag=2^4=16


set_val() {
	echo "val: ${1}"
	if [ "${1}" -lt "0" ]; then
		echo sysctl set_val=$((0x2000+$1))
		sysctl set_val=$((0x2000+$1))
	else 
		echo sysctl set_val=$(($1))
		sysctl set_val=$(($1))
	fi
}
set_cval() {
	echo "val: ${1}"
	if [ "${1}" -lt "0" ]; then
		echo sysctl set_val=$((0x4000+$1))
		sysctl set_val=$((0x4000+$1))
	else 
		echo sysctl set_val=$(($1))
		sysctl set_val=$(($1))
	fi
}
commit_settings() {
	# Enable CSC, instead of bypassing it.
	sysctl -w set_reg=0x1e02008 ; sysctl set_val ; sysctl set_val=0
	# Trigger update
	sysctl -w set_reg=0x1e02004 ; sysctl set_val ; sysctl set_val=$((0x10001))
}


configure_bt601_studio_swing() {
	#Zie https://en.wikipedia.org/wiki/YCbCr#ITU-R_BT.601_conversion
	#
	# Sommetje rekeninghoudende met volledige headroom in Y/U/V:
	# C = Y - 16		Y range: 16..235 = 219 levels
	# D = U - 128		UV range: 16..240 = 224 levels (= 2*112)
	# E = V - 128

	# R = 1.164383 * Y                   + 1.596027 * V  -16*1.164383-128*1.596027
	# G = 1.164383 * Y - (0.391762 * U) - (0.812968 * V) -16*1.164383+128*0.812968+128*0.391762
	# B = 1.164383 * Y +  2.017232 * U                   -16*1.164383-128*2.017232

	#	^*1024		^*1024		^*1024		^:*16


	# Red
	sysctl -w set_reg=0x1e02080 ; sysctl set_val ; set_val +1192		# Y in red		*1024
 	sysctl -w set_reg=0x1e02088 ; sysctl set_val ; set_val +0		# U in red / Cb in red
	sysctl -w set_reg=0x1e02084 ; sysctl set_val ; set_val +1634		# V in red / Cr in red
	sysctl -w set_reg=0x1e0208c ; sysctl set_val ; set_cval -3567		# red constant level	*8

	# Green
	sysctl -w set_reg=0x1e02070 ; sysctl set_val ; set_val +1192		# Y in green
	sysctl -w set_reg=0x1e02074 ; sysctl set_val ; set_val -401		# U in green
	sysctl -w set_reg=0x1e02078 ; sysctl set_val ; set_val -832		# V in green
	sysctl -w set_reg=0x1e0207c ; sysctl set_val ; set_cval +2169		# green constant level	*8

	# Blue
	sysctl -w set_reg=0x1e02090 ; sysctl set_val ; set_val +1192		# Y in blue
	sysctl -w set_reg=0x1e02098 ; sysctl set_val ; set_val 2066		# U in blue
	sysctl -w set_reg=0x1e02094 ; sysctl set_val ; set_val +0		# V in blue
	sysctl -w set_reg=0x1e0209c ; sysctl set_val ; set_cval -4429		# blue constant level *8

	commit_settings
	# Nice!
}


configure_bt601_full_swing() {
	# Zie https://en.wikipedia.org/wiki/YCbCr#ITU-R_BT.601_conversion
	#
	# Sommetje met 'Full swing' Y/U/V input:
	# C = Y 		Y range: 0..255 = 255 levels
	# D = U - 128		UV range: 0..255 = 255 levels (= 2*128)
	# E = V - 128

	# R = 1.000000 * Y               + 0.701 * V   -0*1.0-128*0.701
	# G = 1.000000 * Y - (0.172 * U) - 0.357 * V   -0*1.0+128*0.172+128*0.357
	# B = 1.000000 * Y +  0.886 * U                -0*1.0-128*0.886

	#	^*1024	      ^*1024      ^*1024	^*16

	# Red
	sysctl -w set_reg=0x1e02080 ; sysctl set_val ; set_val +1024		# Y in red		*1024
	sysctl -w set_reg=0x1e02088 ; sysctl set_val ; set_val 0		# U in red
	sysctl -w set_reg=0x1e02084 ; sysctl set_val ; set_val +718		# V in red
	sysctl -w set_reg=0x1e0208c ; sysctl set_val ; set_cval -1436		# red constant level	*16

	# Green
	sysctl -w set_reg=0x1e02070 ; sysctl set_val ; set_val +1024		# Y in green
	sysctl -w set_reg=0x1e02074 ; sysctl set_val ; set_val -176		# U in green
	sysctl -w set_reg=0x1e02078 ; sysctl set_val ; set_val -366		# V in green
	sysctl -w set_reg=0x1e0207c ; sysctl set_val ; set_cval +1083

	# Blue
	sysctl -w set_reg=0x1e02090 ; sysctl set_val ; set_val +1024		# Y in blue
	sysctl -w set_reg=0x1e02098 ; sysctl set_val ; set_val +907		# U in blue
	sysctl -w set_reg=0x1e02094 ; sysctl set_val ; set_val +0		# V in blue
	sysctl -w set_reg=0x1e0209c ; sysctl set_val ; set_cval -1815

	commit_settings
	# Goed plaatje, alleen duidelijk niet full-swing in RGB. Input uit decoder is dus 'studio swing' YUV.
}


configure_bt601_alleen_y_full_swing() {
	#Zie https://en.wikipedia.org/wiki/YCbCr#ITU-R_BT.601_conversion
	#
	# Sommetje met 'Full swing' Y, headroom-U/V input:
	# C = Y 		Y range: 0..255 = 255 levels
	# D = U - 128		UV range: 16..240 = 224 levels (= 2*112)
	# E = V - 128

	# R = 1.000000 * Y               + 1.5960 * V   -0*1.0-128*1.5960
	# G = 1.000000 * Y - 0.3917 * U  - 0.8130 * V   -0*1.0+128*0.3917+128*0.8130
	# B = 1.000000 * Y + 2.0172 * U                 -0*1.0-128*2.0172

	#	^*256*4	      ^*256*4      ^*256*4	^*16

	# Red
	sysctl -w set_reg=0x1e02080 ; sysctl set_val ; set_val +1024		# Y in red		*1024
	sysctl -w set_reg=0x1e02088 ; sysctl set_val ; set_val 0		# U in red
	sysctl -w set_reg=0x1e02084 ; sysctl set_val ; set_val +1634		# V in red
	sysctl -w set_reg=0x1e0208c ; sysctl set_val ; set_cval -3267		# red constant level	*16

	# Green
	sysctl -w set_reg=0x1e02070 ; sysctl set_val ; set_val +1024		# Y in green
	sysctl -w set_reg=0x1e02074 ; sysctl set_val ; set_val -401		# U in green
	sysctl -w set_reg=0x1e02078 ; sysctl set_val ; set_val -833		# V in green
	sysctl -w set_reg=0x1e0207c ; sysctl set_val ; set_cval +2467

	# Blue
	sysctl -w set_reg=0x1e02090 ; sysctl set_val ; set_val +1024		# Y in blue
	sysctl -w set_reg=0x1e02098 ; sysctl set_val ; set_val +2066		# U in blue
	sysctl -w set_reg=0x1e02094 ; sysctl set_val ; set_val +0		# V in blue
	sysctl -w set_reg=0x1e0209c ; sysctl set_val ; set_cval -4131		# blue constant *16

	commit_settings
	# Ook aardig.
}


configure_jpeg_itu_t871_full_swing() {
	# JPEG / ITU-T T.871 matrix met full swing Y/U/V:
	# C = Y 		Y range: 0..255 = 255 levels
	# D = U - 128		UV range: 0..255 = 255 levels (= 2*128)
	# E = V - 128

	# R = 1.000000 * Y               + 1.4020 * V   -0*1.0-128*1.4020
	# G = 1.000000 * Y - 0.3441 * U  - 0.7141 * V   -0*1.0+128*0.3441+128*0.7141
	# B = 1.000000 * Y + 1.7720 * U                 -0*1.0-128*1.7720

	#	^*256*4	      ^*256*4      ^*256*4		^*16

	# Red
	sysctl -w set_reg=0x1e02080 ; sysctl set_val ; set_val +1024		# Y in red		*1024
	sysctl -w set_reg=0x1e02088 ; sysctl set_val ; set_val +0		# U in red
	sysctl -w set_reg=0x1e02084 ; sysctl set_val ; set_val +1436		# V in red
	sysctl -w set_reg=0x1e0208c ; sysctl set_val ; set_cval -2871		# red constant level	*16

	# Green
	sysctl -w set_reg=0x1e02070 ; sysctl set_val ; set_val +1024		# Y in green
	sysctl -w set_reg=0x1e02074 ; sysctl set_val ; set_val -352		# U in green
	sysctl -w set_reg=0x1e02078 ; sysctl set_val ; set_val -731		# V in green
	sysctl -w set_reg=0x1e0207c ; sysctl set_val ; set_cval +2167

	# Blue
	sysctl -w set_reg=0x1e02090 ; sysctl set_val ; set_val +1024		# Y in blue
	sysctl -w set_reg=0x1e02098 ; sysctl set_val ; set_val +1815		# U in blue
	sysctl -w set_reg=0x1e02094 ; sysctl set_val ; set_val +0		# V in blue
	sysctl -w set_reg=0x1e0209c ; sysctl set_val ; set_cval +3629

	commit_settings
	# Teveel blauw in de lichte delen van 't beeld.
}


configure_bt709_studio_swing() {
	# Zie http://www.equasys.de/colorconversion.html - HDTV / ITU-R BT.709
	#
	# Sommetje rekeninghoudende met volledige headroom in Y/U/V:
	# C = Y - 16		Y range: 16..235 = 219 levels
	# D = U - 128		UV range: 16..240 = 224 levels (= 2*112)
	# E = V - 128

	# R = 1.164383 * Y              + 1.793 * V  -16*1.164383-128*1.793
	# G = 1.164383 * Y - 0.213 * U  - 0.533 * V  -16*1.164383+128*0.213+128*0.533
	# B = 1.164383 * Y + 2.122 * U               -16*1.164383-128*2.122

	#     ^*1024         ^*1024       ^*1024      ^*16

	# Red
	sysctl -w set_reg=0x1e02080 ; sysctl set_val ; set_val +1192		# Y in red		*1024
	sysctl -w set_reg=0x1e02088 ; sysctl set_val ; set_val +0		# U in red
	sysctl -w set_reg=0x1e02084 ; sysctl set_val ; set_val +1836		# V in red
	sysctl -w set_reg=0x1e0208c ; sysctl set_val ; set_cval -3970		# red constant level	*16

	# Green
	sysctl -w set_reg=0x1e02070 ; sysctl set_val ; set_val +1192		# Y in green
	sysctl -w set_reg=0x1e02074 ; sysctl set_val ; set_val -218		# U in green
	sysctl -w set_reg=0x1e02078 ; sysctl set_val ; set_val -546		# V in green
	sysctl -w set_reg=0x1e0207c ; sysctl set_val ; set_cval +1230

	# Blue
	sysctl -w set_reg=0x1e02090 ; sysctl set_val ; set_val +1192		# Y in blue
	sysctl -w set_reg=0x1e02098 ; sysctl set_val ; set_val +2173		# U in blue
	sysctl -w set_reg=0x1e02094 ; sysctl set_val ; set_val +0		# V in blue
	sysctl -w set_reg=0x1e0209c ; sysctl set_val ; set_cval -4644

	commit_settings
	#(Bij Big Buck bunny is hiermee 't felle rood wat gelig.)
}


configure_bt709_full_swing() {
	# Zie http://www.equasys.de/colorconversion.html - HDTV / ITU-R BT.709
	#
	# Sommetje rekeninghoudende met full-swing in Y/U/V:
	# C = Y - 0		Y range: 0..255 = 255 levels
	# D = U - 128		UV range: 0..255 = 255 levels (= 2*128)
	# E = V - 128

	# R = 1.000 * Y              + 1.575 * V  -0*1.000-128*1.575
	# G = 1.000 * Y - 0.187 * U  - 0.468 * V  -0*1.000+128*0.187+128*0.468
	# B = 1.000 * Y + 1.856 * U               -0*1.000-128*1.856

	#     ^*1024      ^*1024       ^*1024      ^*16

	# Red
	sysctl -w set_reg=0x1e02080 ; sysctl set_val ; set_val +1024		# Y in red		*1024
	sysctl -w set_reg=0x1e02088 ; sysctl set_val ; set_val +0		# U in red
	sysctl -w set_reg=0x1e02084 ; sysctl set_val ; set_val +1613		# V in red
	sysctl -w set_reg=0x1e0208c ; sysctl set_val ; set_cval -3226		# red constant level	*16

	# Green
	sysctl -w set_reg=0x1e02070 ; sysctl set_val ; set_val +1024		# Y in green
	sysctl -w set_reg=0x1e02074 ; sysctl set_val ; set_val -191		# U in green
	sysctl -w set_reg=0x1e02078 ; sysctl set_val ; set_val -479		# V in green
	sysctl -w set_reg=0x1e0207c ; sysctl set_val ; set_cval +1341

	# Blue
	sysctl -w set_reg=0x1e02090 ; sysctl set_val ; set_val +1024		# Y in blue
	sysctl -w set_reg=0x1e02098 ; sysctl set_val ; set_val +1901		# U in blue
	sysctl -w set_reg=0x1e02094 ; sysctl set_val ; set_val +0		# V in blue
	sysctl -w set_reg=0x1e0209c ; sysctl set_val ; set_cval -3801

	commit_settings
	#(Ook wel aardig. Duidelijk dat de kleur geen full-swing maakt nu. Rood is een tikkie gelig.)
}


configure_bt601_bogus_testje() {
	#Zie https://en.wikipedia.org/wiki/YCbCr#ITU-R_BT.601_conversion
	#
	# Sommetje rekeninghoudende met volledige headroom in Y/U/V en een converter die alvast 16 van Y afhaalt aan de input.
	# C = Y 		Y range: 0..219 = 219 levels
	# D = U - 128		UV range: 16..240 = 224 levels (= 2*112)
	# E = V - 128

	# R = 1.164383 * Y                   + 1.596027 * V  -128*1.596027
	# G = 1.164383 * Y - (0.391762 * U) - (0.812968 * V) +128*0.812968+128*0.391762
	# B = 1.164383 * Y +  2.017232 * U                   -128*2.017232

	#	^*1024	      ^*1024	       ^*1024	      ^*16

	# Red
	sysctl -w set_reg=0x1e02080 ; sysctl set_val ; set_val +1192		# Y in red		*1024
	sysctl -w set_reg=0x1e02088 ; sysctl set_val ; set_val +0		# U in red
	sysctl -w set_reg=0x1e02084 ; sysctl set_val ; set_val +1634		# V in red
	sysctl -w set_reg=0x1e0208c ; sysctl set_val ; set_cval -3267		# red constant level	*16

	# Green
	sysctl -w set_reg=0x1e02070 ; sysctl set_val ; set_val +1192		# Y in green
	sysctl -w set_reg=0x1e02074 ; sysctl set_val ; set_val -401		# U in green
	sysctl -w set_reg=0x1e02078 ; sysctl set_val ; set_val -832		# V in green
	sysctl -w set_reg=0x1e0207c ; sysctl set_val ; set_cval +2467		# green constant

	# Blue
	sysctl -w set_reg=0x1e02090 ; sysctl set_val ; set_val +1192		# Y in blue
	sysctl -w set_reg=0x1e02098 ; sysctl set_val ; set_val 2066		# U in blue
	sysctl -w set_reg=0x1e02094 ; sysctl set_val ; set_val +0		# V in blue
	sysctl -w set_reg=0x1e0209c ; sysctl set_val ; set_cval -4131		# blue constant

	commit_settings
	# Rood wel erg gelig.
}


configure_pal_matrix_studio_swing() {
	#Zie http://www.equasys.de/colorconversion.html
	#
	# Oer analoge PAL/NTSC matrix, opgeschaald naar 8-bit Y/U/V met headroom:
	# C = Y - 16		Y range: 16..235 = 219 levels
	# D = U - 128		UV range: 16..240 = 224 levels (= 2*112)
	# E = V - 128

	# R = 1.164 * Y              +1.140 * V  -16*1.164-128*1.140
	# G = 1.164 * Y  -0.395 * U  -0.581 * V  -16*1.164+128*0.395+128*0.581
	# B = 1.164 * Y  +2.032 * U              -16*1.164-128*2.032

	#     ^*1024      ^*1024      ^*1024      ^*16

	# Red
	sysctl -w set_reg=0x1e02080 ; sysctl set_val ; set_val +1192		# Y in red		*1024
	sysctl -w set_reg=0x1e02088 ; sysctl set_val ; set_val +0		# U in red
	sysctl -w set_reg=0x1e02084 ; sysctl set_val ; set_val +1167		# V in red
	sysctl -w set_reg=0x1e0208c ; sysctl set_val ; set_cval -2633		# red constant level	*16

	# Green
	sysctl -w set_reg=0x1e02070 ; sysctl set_val ; set_val +1192		# Y in green
	sysctl -w set_reg=0x1e02074 ; sysctl set_val ; set_val -404		# U in green
	sysctl -w set_reg=0x1e02078 ; sysctl set_val ; set_val -595		# V in green
	sysctl -w set_reg=0x1e0207c ; sysctl set_val ; set_cval +1701

	# Blue
	sysctl -w set_reg=0x1e02090 ; sysctl set_val ; set_val +1192		# Y in blue
	sysctl -w set_reg=0x1e02098 ; sysctl set_val ; set_val +2081		# U in blue
	sysctl -w set_reg=0x1e02094 ; sysctl set_val ; set_val +0		# V in blue
	sysctl -w set_reg=0x1e0209c ; sysctl set_val ; set_cval -4460

	commit_settings
	# Ook een mooi plaatje. Kleuren hadden iets intenser gemogen.
}


configure_bt601_studio_swing_in_headroom_out() {
	#Zie https://en.wikipedia.org/wiki/YCbCr#ITU-R_BT.601_conversion
	#
	# Sommetje rekeninghoudende met volledige headroom in Y/U/V aan zowel ingang als uitgang. (R/G/B=16 is zwart)
	# C = Y - 16		Y range: 16..235 = 219 levels
	# D = U - 128		UV range: 16..240 = 224 levels (= 2*112)
	# E = V - 128

	# R = 1.000 * Y              +1.371 * V  -0*1.000-128*1.371
	# G = 1.000 * Y  -0.336 * U  -0.698 * V  -0*1.000+128*0.336+128*0.698
	# B = 1.000 * Y  +1.732 * U              -0*1.000-128*1.732

	#     ^*1024      ^*1024      ^*1024      ^*16

	# Red
	sysctl -w set_reg=0x1e02080 ; sysctl set_val ; set_val +1024		# Y in red		*1024
	sysctl -w set_reg=0x1e02088 ; sysctl set_val ; set_val 0		# U in red
	sysctl -w set_reg=0x1e02084 ; sysctl set_val ; set_val +1404		# V in red
	sysctl -w set_reg=0x1e0208c ; sysctl set_val ; set_cval -2808		# red constant level	*16

	# Green
	sysctl -w set_reg=0x1e02070 ; sysctl set_val ; set_val +1024		# Y in green
	sysctl -w set_reg=0x1e02074 ; sysctl set_val ; set_val -334		# U in green
	sysctl -w set_reg=0x1e02078 ; sysctl set_val ; set_val -714		# V in green
	sysctl -w set_reg=0x1e0207c ; sysctl set_val ; set_cval +2118

	# Blue
	sysctl -w set_reg=0x1e02090 ; sysctl set_val ; set_val +1024		# Y in blue
	sysctl -w set_reg=0x1e02098 ; sysctl set_val ; set_val +1774		# U in blue
	sysctl -w set_reg=0x1e02094 ; sysctl set_val ; set_val +0		# V in blue
	sysctl -w set_reg=0x1e0209c ; sysctl set_val ; set_cval -3547

	commit_settings
	#(Ziet er ook aardig uit.)
}


configure_nv21_matrix_studio_swing() {
	#Zie https://en.wikipedia.org/wiki/YUV#Converting_between_Y.E2.80.B2UV_and_RGB
	#
	# YUV420sp (NV21) format:
	# C = Y			Y range: 16..235 = 219 levels
	# D = U - 128		UV range: 16..240 = 224 levels (= 2*112)
	# E = V - 128

	# R = 1.000 * Y              +1.371 * V  -0*1.000-128*1.371
	# G = 1.000 * Y  -0.336 * U  -0.698 * V  -0*1.000+128*0.336+128*0.698
	# B = 1.000 * Y  +1.732 * U              -0*1.000-128*1.732

	#     ^*1024      ^*1024      ^*1024      ^*16

	# Red
	sysctl -w set_reg=0x1e02080 ; sysctl set_val ; set_val +1024		# Y in red		*1024
	sysctl -w set_reg=0x1e02088 ; sysctl set_val ; set_val 0		# U in red
	sysctl -w set_reg=0x1e02084 ; sysctl set_val ; set_val +1404		# V in red
	sysctl -w set_reg=0x1e0208c ; sysctl set_val ; set_cval -2808		# red constant level	*16

	# Green
	sysctl -w set_reg=0x1e02070 ; sysctl set_val ; set_val +1024		# Y in green
	sysctl -w set_reg=0x1e02074 ; sysctl set_val ; set_val -334		# U in green
	sysctl -w set_reg=0x1e02078 ; sysctl set_val ; set_val -714		# V in green
	sysctl -w set_reg=0x1e0207c ; sysctl set_val ; set_cval +2118

	# Blue
	sysctl -w set_reg=0x1e02090 ; sysctl set_val ; set_val +1024		# Y in blue
	sysctl -w set_reg=0x1e02098 ; sysctl set_val ; set_val +1774		# U in blue
	sysctl -w set_reg=0x1e02094 ; sysctl set_val ; set_val +0		# V in blue
	sysctl -w set_reg=0x1e0209c ; sysctl set_val ; set_cval -3547

	commit_settings
	#(Ook ok.)
}


configure_r_min_y_b_min_y_test() {
	# Standaard-loze test met simpelweg U = R-Y / V = B-Y
	#
	# Y = 0.333*R + 0.333*G + 0.333*B
	# U = R - Y
	# V = B - Y
	#
	# R = 1.164383 * Y              +1.164 * V  -16*1.164383-128*1.164
	# G = 1.164383 * Y  -0.823 * U  -0.823 * V  -16*1.164383+128*0.823+128*0.823
	# B = 1.164383 * Y  +1.164 * U              -16*1.164383-128*1.164

	#	^*256*4		^*256*4		^*256*4		^*8

	# Red
	sysctl -w set_reg=0x1e02080 ; sysctl set_val ; set_val +1192		# Y in red		*1024
	sysctl -w set_reg=0x1e02088 ; sysctl set_val ; set_val +0		# U in red
	sysctl -w set_reg=0x1e02084 ; sysctl set_val ; set_val +1192		# V in red
	sysctl -w set_reg=0x1e0208c ; sysctl set_val ; set_cval -2682		# red constant level	*8

	# Green
	sysctl -w set_reg=0x1e02070 ; sysctl set_val ; set_val +1192		# Y in green
	sysctl -w set_reg=0x1e02074 ; sysctl set_val ; set_val -843		# U in green
	sysctl -w set_reg=0x1e02078 ; sysctl set_val ; set_val -843		# V in green
	sysctl -w set_reg=0x1e0207c ; sysctl set_val ; set_cval +3073

	# Blue
	sysctl -w set_reg=0x1e02090 ; sysctl set_val ; set_val +1192		# Y in blue
	sysctl -w set_reg=0x1e02098 ; sysctl set_val ; set_val +1192		# U in blue
	sysctl -w set_reg=0x1e02094 ; sysctl set_val ; set_val +0		# V in blue
	sysctl -w set_reg=0x1e0209c ; sysctl set_val ; set_cval +2682

	commit_settings
	#(Veel te blauw)
}


configure_a20_mixer_proc_default_csc_matrix_uit_manual() {
	# Test:
	#
	# See A20 user manual V1.4, page 340...
	# Uit de manual gerukte default waarden van de CSC core in de mixer unit...
	# Enkel even gereshuffled om de registervolgorde te matchen met de volgorde in DEFE.

	# Red
	sysctl -w set_reg=0x1e02080 ; sysctl set_val ; set_val $((0x4a7))	# Y in red		*1024
	sysctl -w set_reg=0x1e02088 ; sysctl set_val ; set_val $((0x0))		# U in red
	sysctl -w set_reg=0x1e02084 ; sysctl set_val ; set_val $((0x662))	# V in red
	sysctl -w set_reg=0x1e0208c ; sysctl set_val ; set_cval $((0x3211))	# red constant level	*8

	# Green
	sysctl -w set_reg=0x1e02070 ; sysctl set_val ; set_val $((0x4a7))	# Y in green
	sysctl -w set_reg=0x1e02078 ; sysctl set_val ; set_val $((0x1cbf))	# U in green
	sysctl -w set_reg=0x1e02074 ; sysctl set_val ; set_val $((0x1e6f))	# V in green
	sysctl -w set_reg=0x1e0207c ; sysctl set_val ; set_cval $((0x877))	# green constant

	# Blue
	sysctl -w set_reg=0x1e02090 ; sysctl set_val ; set_val $((0x4a7))	# Y in blue
	sysctl -w set_reg=0x1e02098 ; sysctl set_val ; set_val $((0x812))	# U in blue
	sysctl -w set_reg=0x1e02094 ; sysctl set_val ; set_val $((0x0))		# V in blue
	sysctl -w set_reg=0x1e0209c ; sysctl set_val ; set_cval $((0x2eb1))	# blue constant

	# En nu decimaal:

	# Red
	sysctl -w set_reg=0x1e02080 ; sysctl set_val ; set_val +1191		# Y in red		*1024
	sysctl -w set_reg=0x1e02088 ; sysctl set_val ; set_val +0		# U in red
	sysctl -w set_reg=0x1e02084 ; sysctl set_val ; set_val +1634		# V in red
	sysctl -w set_reg=0x1e0208c ; sysctl set_val ; set_cval -3567		# red constant level	*8

	# Green
	sysctl -w set_reg=0x1e02070 ; sysctl set_val ; set_val +1191		# Y in green
	sysctl -w set_reg=0x1e02078 ; sysctl set_val ; set_val -833		# U in green
	sysctl -w set_reg=0x1e02074 ; sysctl set_val ; set_val -401		# V in green
	sysctl -w set_reg=0x1e0207c ; sysctl set_val ; set_cval +2167		# green constant

	# Blue
	sysctl -w set_reg=0x1e02090 ; sysctl set_val ; set_val +1191		# Y in blue
	sysctl -w set_reg=0x1e02098 ; sysctl set_val ; set_val +2066		# U in blue
	sysctl -w set_reg=0x1e02094 ; sysctl set_val ; set_val +0		# V in blue
	sysctl -w set_reg=0x1e0209c ; sysctl set_val ; set_cval -4428		# blue constant

	commit_settings
	#(Veel te rood)
}




# Maak er hieronder 1 actief en run dit script.
# (Of source 't script met '. ./test.sh' en copy/paste daarna een willekeurige
# functie uit de lijst hieronder in je terminal om makkelijk even snel tussen
# matrixen te kunnen switchen...

#configure_bt601_studio_swing
commit_settings
#configure_bt601_full_swing
#configure_bt601_alleen_y_full_swing
#configure_jpeg_itu_t871_full_swing
#configure_bt709_studio_swing
#configure_bt709_full_swing
#configure_bt601_bogus_testje
#configure_pal_matrix_studio_swing
#configure_bt601_studio_swing_in_headroom_out
#configure_nv21_matrix_studio_swing
#configure_r_min_y_b_min_y_test
#configure_a20_mixer_proc_default_csc_matrix_uit_manual






