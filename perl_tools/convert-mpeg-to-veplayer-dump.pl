#!/usr/bin/perl

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

use strict;

# This tool can be used to turn MPEG-2 files into Thomas' ve_player dump file
# format. Full buffer management of forward and backward reference frames is
# implemented and it will cycle through all 4 of the output buffers so we have
# one buffer that will always stay in-tact which can be sent to the display
# front end without getting corrupted before it's time to show the next
# picture.
#
# Run the tool like this:
#	./convert-mpeg-to-veplayer-dump.pl < ./input-file.mpeg
#
# The tool uses 'dvbsnoop' to do most of the MPEG splitting, so it expects to
# be able to run 'dvbsnoop' from the $PATH.
#
# The dump that this file produces can be successfully turned back into an MPEG
# movie using 'parse-vaapi-dump.pl'.
#

my $skip_gops = 0;
my $out_fname = 'new-dump.bin';

while (@ARGV) {
	my $arg = shift(@ARGV);
	
	if ($arg eq '-skip-gops') {
		$skip_gops = shift(@ARGV);
		next;
	}
	if ($arg eq '-out') {
		$out_fname = shift(@ARGV);
		next;
	}
	
	die "Unhandled argument '${arg}'!";
}

my $VAPictureParameterBufferType = 0,
my $VAIQMatrixBufferType = 1;
my $VASliceParameterBufferType = 4;
my $VASliceDataBufferType = 5;

my $have_picture_parameters = 0;
my $width;
my $height;
my $fps;
my $pkt_picture_parameters;

my $in_slice = 0;
my $slice_str = '';
my $slice_parameters = '';
my $data_all_slices = '';
my $nr_of_slices = 0;

my $picture_coding_type = '';
my $temporal_reference;
my $last_temporal_reference = '';

# Start with all output buffers being free.
my @unused_bufs = (0, 1, 2, 3);
my $forward_idx = '';
my $backward_idx = '';
my $backward_temporal_reference = '';

my $f_code = 0xffff;

my $line0000;
my $seen_gops = 0;
my $last_line;

# We use dvbsnoop to do the heavy-lifting for us. Don't try to write yourself
# what you can readily pull into Perl... :-)
my $in;
open($in, '-|', 'dvbsnoop -buffersize 50000 -s pes -if /dev/stdin');

my $out;
open($out, '>', $out_fname)
    or die "Failed to open output '${out_fname}': $!";

my $line;
while ($line = <$in>) {
	$line =~ s/[\r\n]//g;
	
	#print "[${line}]\n";
	
	# First, we'll need basic info about the movie. Parse lines untill we
	# find it.
	if (! $have_picture_parameters) {
		if ($line =~ m/^\s*horizontal_size_value:\ (\d+)/) {
			$width = $1;
		}
		if ($line =~ m/^\s*vertical_size_value:\ (\d+)/) {
			$height = $1;
		}
		if ($line =~ m/^\s*frame_rate_code:.*=\ (\d+)/) {
			$fps = $1;
			$have_picture_parameters = 1;
			# Construct the VAPictureParameterBufferType packet
			# now. It stays constant for the duration of the movie.

		}
	}
	
	if ($skip_gops > 0) {
		if ($line =~ /group_start_code/) {
			$skip_gops--;
			if ($skip_gops > 0) {
				next;
			}
		}
		next;
	}
	
	if ($line =~ m/^\s*temporal_reference: (\d+)/) {
		$temporal_reference = $1;
	}
	if ($line =~ m/^\s*picture_coding_type.*\((.)\)\]/) {
		$picture_coding_type = $1;
		printf("temporal_reference=%d, type=%s, f_code=0x%04x\n", $temporal_reference,
		    $picture_coding_type, $f_code);
	}
	
	if ($line =~ /^\s*MPEG-2\ Slice/) {
		$in_slice = 1;
	}
	if ($line =~ /^=====/) {
		$in_slice = 0;
		if ($slice_str ne '') {
			# We've captured the data of one slice in dvbsnoop's
			# textual output format. Add the block's info to the
			# slice parameter buffer and add it's data to the data
			# block.
			add_slice($slice_str);
			$slice_str = '';
		}
	}
	if ($in_slice) {
		$slice_str .= $line ."\n";
	}
	
	if ($line =~ /^\s*f_code\[/) {
		parse_f_code_line($line);
	}
	
	if (($line =~ /^\ \ *0000:/) && ($seen_gops)) {
		$line0000 = $line;
	}
	if (($line eq 'Stream_id: 179 (0xb3)  [= sequence_header_code]') &&
	    ($seen_gops)) {
	#	add_slice($line0000);
	}
	if (($last_line eq 'Stream_id: 181 (0xb5)  [= extension_start_code]') &&
	    #Sequence Extension ID
	    ($line ne '    extension_start_code_identifier: 2 (0x02)  [= Sequence Display Extension ID]') &&
	    ($line ne '    extension_start_code_identifier: 8 (0x08)  [= Picture Coding Extension ID]') &&
	    ($seen_gops)) {
	#	add_slice($line0000);
	}
	$last_line = $line;
	

	# At the start of a new GOP or a new picture, dump the contents of the
	# previous picture that we've gathered in memory.
	if ($line =~ /^Stream_id.*(group_start_code|picture_start_code)/) {
		dump_picture($out, $temporal_reference, $picture_coding_type);
		$seen_gops++;
	}
}

close($out);
exit(0);

sub claim_buffer {
	if (@unused_bufs == 0) {
		die "EEK! Ran out of unused buffers!";
	}
	return shift(@unused_bufs);
}
sub release_buffer {
	my ($idx) = @_;

	push(@unused_bufs, $idx);
}

sub parse_f_code_line {
	my ($line) = @_;
	
	$line =~ m/f_code\[(forward|backward)\]\[(horizontal|vertical)\]: (\d+)/;
	my ($t, $d, $num) = ($1, $2, $3);
	$t = ($t eq 'forward') ? 8 : 0;
	$d = ($d eq 'horizontal') ? 4 : 0;
	
	my $shift = $t + $d;
	my $mask = 0xf << $shift;
	
	$f_code = $f_code & ~$mask;
	$f_code |= $num << $shift;
}


sub add_slice {
	my ($slice_str) = @_;
	my $slice_data;
	my $slice_size;
	my $slice_offset;
	
	# Get binary form of data.
	$slice_data = dvbsnoop_pkt_dump_to_bin($slice_str);
	$slice_size = length($slice_data);
	$slice_offset = length($data_all_slices);

	# See https://sourcecodebrowser.com/libva/1.0.4/va_8h_source.html
	# line 594.
	
	# Add size+offset and other constant info to slice parameter buffer.
	$slice_parameters .= pack('IIIIIIII',
	    $slice_size, length($data_all_slices),
	    0, 38, 0, 4, 0, 0,
	);
	
	# Add this slice's data to the data buffer.
	$data_all_slices .= $slice_data;
	$nr_of_slices++;
	
	# (We'll only add data to variables here. Dumping the buffers will only
	# happen when we've come across the end of the complete picture.)
}

sub dump_picture {
	my ($fh, $temporal_reference, $frame_type) = @_;
	my $out_buf_idx;
	my $forward_buf_idx;
	my $backward_buf_idx;
	my $temporal_ofs;
	my $set_this_picture_as_new_forward_buf;
	my $set_this_picture_as_new_backward_buf;
	
	# (Here we dump all buffers / slices / data for this picture.)
	
	# If we have one...
	if ($nr_of_slices == 0) {
		return;
	}
	
	# If we're at the start of a new GOP and this is an I frame, we don't
	# need back/forward references we might still have around any more.
	if (($temporal_reference == 0) && ($frame_type eq 'I')) {
		if ($forward_idx ne '') {
			printf("  Releasing forward_idx=%d on new GOP\n",
			    $forward_idx);
			release_buffer($forward_idx);
			$forward_idx = '';
		}
		if ($backward_idx ne '') {
			printf("  Releasing backward_idx=%d on new GOP\n",
			    $backward_idx);
			release_buffer($backward_idx);
			$backward_idx = '';
		}
	}
	
	# First, if this is a new I frame, or a P frame following an I frame
	# with a temporal offset of exactly +1 (= no more frames in between to
	# come), release any old 'forward buffer' that we might still have
	# around.
	# TODO: Should tell ve_player that now is the right moment to DQ this
	# output buffer somehow.
	$temporal_ofs = $temporal_reference - $last_temporal_reference;
	$last_temporal_reference = $temporal_reference;
	if ($frame_type eq 'I') {
		$set_this_picture_as_new_forward_buf = 1;
	}
	if (($frame_type eq 'P') && ($temporal_ofs == +1)) {
		$set_this_picture_as_new_forward_buf = 1;
	}
	
	# Check for new/old backward reference frames.
	if ($temporal_ofs > 1) {
		# We're skipping frames forward in time. This must mean that
		# we're about to decode this picture as a new backward
		# reference picture so we can decode earlier pictures
		# afterwards. Make sure we don't release this buffer and keep
		# it around as new backward reference.
		$set_this_picture_as_new_backward_buf = 1;
	}
	
	# Start by determining which output buffer index we can use to decode
	# this picture into.
	my $out_buf_idx = claim_buffer();
	
	printf("Dumping picture: type=%s,\n".
	    " temporal_ref=%d, buf_idx=%d, forward_idx=%s, backward_idx=%s\n",
	    $frame_type,
	    $temporal_reference, $out_buf_idx, $forward_idx, $backward_idx);
	
	# Start every picture with a picture parameter buffer.
	write_picture_param_buffer($fh, $out_buf_idx, $frame_type,
	    $forward_idx, $backward_idx);
	
	# Then add the IQ matrix.
	write_iq_matrix_buffer($fh, $out_buf_idx);
	
	# Then add the slice parameter buffer that we've constructed.
	# (This is basically just a list of offsets and sizes into the
	# following data buffer that tells the decoder where individual slices
	# start and stop in the data buffer.)
	write_slice_parameter_buffer($fh, $out_buf_idx);
	write_slice_data_buffer($fh, $out_buf_idx);
	

	if ($set_this_picture_as_new_forward_buf) {
		# We'll set this picture as new forward buffer to use from this
		# picture onwards. We'll keep it claimed to make sure we don't
		# overwrite it with the pixels of any future pictures.
		
		if ($forward_idx ne '') {
			# Release old one first.
			printf("  Releasing forward_idx=%d because of new ".
			    " one.\n", $forward_idx);
			release_buffer($forward_idx);
			$forward_idx = '';
		}
		
		$forward_idx = $out_buf_idx;
		printf(" Setting this picture (buf_idx=%d) as new forward ".
		    "reference.\n", $out_buf_idx);
	}
	if ($set_this_picture_as_new_backward_buf) {
		# We'll set this picture as new backward buffer to use from
		# this picture onwards. The decoder will now decode pictures
		# from before the moment this picture was taken.

		if ($backward_idx ne '') {
			# Release old one first.
			printf("  Releasing backward_idx=%d because of new ".
			    "one.\n", $backward_idx);
			release_buffer($backward_idx);
			$backward_idx = '';
		}
		
		$backward_idx = $out_buf_idx;
		$backward_temporal_reference = $temporal_reference;
		printf(" Setting this picture (buf_idx=%d) as new backward ".
		    "reference.\n", $out_buf_idx);
	}
	
	if ((! $set_this_picture_as_new_forward_buf) &&
	    (! $set_this_picture_as_new_forward_buf)) {
		# Seems noone needs this picture any more when it's been
		# decoded. Release it.
		release_buffer($out_buf_idx);
		printf(" Releasing buffer (buf_idx=%d) when decoder is ".
		    "finished.\n", $out_buf_idx);
	}
	
	print "\n";
}

sub write_buffer {
	my ($fh, $buf_idx, $buf_type, $num_elements, $data) = @_;
	my $buf;
	
	my $size = length($data);
	
	$buf = pack('LLLL', $buf_idx, $buf_type, $size, $num_elements);
	$buf .= $data;
	
	print $fh $buf;
}

sub write_slice_parameter_buffer {
	my ($fh, $buf_idx) = @_;
	
	# See https://sourcecodebrowser.com/libva/1.0.4/va_8h_source.html
	# line 594.
	write_buffer($fh, $buf_idx, $VASliceParameterBufferType, $nr_of_slices,
	    $slice_parameters);
}

sub write_slice_data_buffer {
	my ($fh, $buf_idx) = @_;

	write_buffer($fh, $buf_idx, $VASliceDataBufferType, 1,
	    $data_all_slices);
	
	$slice_parameters = '';
	$data_all_slices = 0;
	$nr_of_slices = 0;
}

sub write_iq_matrix_buffer {
	my ($fh, $buf_idx) = @_;
	my $pkt;
	
	# Assumption here: The input movie will always use the ITU-R BT.709
	# recommended quantisation matrices.

	my $load_intra_quantiser_matrix = 1;
	my $load_non_intra_quantiser_matrix = 1;
	my $load_chroma_intra_quantiser_matrix = 1;
	my $load_chroma_non_intra_quantiser_matrix = 1;
	
	# See https://sourcecodebrowser.com/libva/1.0.4/va_8h_source.html
	# line 581
	$pkt = pack('IIIIC*',
	    $load_intra_quantiser_matrix,
	    $load_non_intra_quantiser_matrix,
	    $load_chroma_intra_quantiser_matrix,
	    $load_chroma_non_intra_quantiser_matrix,
	    # intra quantiser matrix:
	    8, 16, 16, 19, 16, 19, 22, 22, 22, 22, 22, 22, 26, 24, 26, 27, 27,
	    27, 26, 26, 26, 26, 27, 27, 27, 29, 29, 29, 34, 34, 34, 29, 29, 29,
	    27, 27, 29, 29, 32, 32, 34, 34, 37, 38, 37, 35, 35, 34, 35, 38, 38,
	    40, 40, 40, 48, 48, 46, 46, 56, 56, 58, 69, 69, 83,
	    # non intra quantiser matrix:
	    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
	    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
	    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
	    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
	    #, chroma, intra, quantiser, matrix:
	    8, 16, 16, 19, 16, 19, 22, 22, 22, 22, 22, 22, 26, 24, 26, 27, 27,
	    27, 26, 26, 26, 26, 27, 27, 27, 29, 29, 29, 34, 34, 34, 29, 29, 29,
	    27, 27, 29, 29, 32, 32, 34, 34, 37, 38, 37, 35, 35, 34, 35, 38, 38,
	    40, 40, 40, 48, 48, 46, 46, 56, 56, 58, 69, 69, 83,
	    #, chroma, non, intra, quantiser, matrix:
	    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
	    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
	    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
	    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
	);

	# Test
	$pkt = pack('IIIIC*',
	    $load_intra_quantiser_matrix,
	    $load_non_intra_quantiser_matrix,
	    $load_chroma_intra_quantiser_matrix,
	    $load_chroma_non_intra_quantiser_matrix,
	    # intra quantiser matrix:
	    8,17,17,19,17,19,21,21,21,21,21,21,23,22,23,24,24,24,24,24,24,24,
	    25,25,25,26,26,26,29,29,29,26,26,26,25,25,27,27,28,29,29,29,31,32,
	    32,31,31,30,30,33,33,34,34,34,38,38,37,37,43,43,44,50,50,58,
	    # non intra quantiser matrix:
	    16,17,17,18,18,18,19,19,19,19,20,20,20,20,20,21,21,21,21,21,21,22,
	    22,22,22,22,22,22,23,23,23,23,23,23,23,23,24,24,24,25,24,24,24,25,
	    26,26,26,26,25,27,27,27,27,27,28,28,28,28,30,30,30,31,31,33,
	    # chroma, intra, quantiser, matrix:
	    8,17,17,19,17,19,21,21,21,21,21,21,23,22,23,24,24,24,24,24,24,24,
	    25,25,25,26,26,26,29,29,29,26,26,26,25,25,27,27,28,29,29,29,31,32,
	    32,31,31,30,30,33,33,34,34,34,38,38,37,37,43,43,44,50,50,58,
	    # chroma, non intra, quantiser, matrix:
	    16, 17, 17, 18, 18, 18, 19, 19, 19, 19, 20, 20, 20, 20, 20, 21, 21,
	    21, 21, 21, 21, 22, 22, 22, 22, 22, 22, 22, 23, 23, 23, 23, 23, 23,
	    23, 23, 24, 24, 24, 25, 24, 24, 24, 25, 26, 26, 26, 26, 25, 27, 27,
	    27, 27, 27, 28, 28, 28, 28, 30, 30, 30, 31, 31, 33,
	);
	
	write_buffer($fh, $buf_idx, $VAIQMatrixBufferType, 1, $pkt);
}

sub write_picture_param_buffer {
	my ($fh, $buf_idx, $frame_type, $forward_buf_idx,
	    $backwards_buf_idx) = @_;
	my $pkt;
	my $picture_coding_type;
	#my $f_code;
	my $picture_coding_extension;
	
	if ($frame_type eq 'I') {
		$picture_coding_type = 1;
		#$f_code = 65535;
		$picture_coding_extension = 6188;
	} elsif ($frame_type eq 'P') {
		$picture_coding_type = 2;
		#$f_code = 4607;
		$picture_coding_extension = 6188;
	} else {
		die "EEK! Teach me how to generate '${frame_type}' frames ".
		    "first!";
	}
	
	if ($forward_buf_idx ne '') {
		$forward_buf_idx |= 0x4000000;
	} else {
		$forward_buf_idx = 0xffffffff;
	}
	if ($backwards_buf_idx ne '') {
		$backwards_buf_idx |= 0x4000000;
	} else {
		$backwards_buf_idx = 0xffffffff;
	}
	
	# See: https://sourcecodebrowser.com/libva/1.0.4/va_8h_source.html
	# line 551
	$pkt = pack('SSIIIII',
	    $width, $height,
	    $forward_buf_idx, $backwards_buf_idx,
	    $picture_coding_type, $f_code, $picture_coding_extension
	);
	
	write_buffer($fh, $buf_idx, $VAPictureParameterBufferType, 1, $pkt);
}


sub dvbsnoop_pkt_dump_to_bin {
	my ($str) = @_;
	my $line;
	my $blob;
	
	foreach $line (split(/\r?\n/, $str)) {
		# Cut leading/trailing whitespace.
		$line =~ s/^\s*|\s*$//g;
		# Only interested in lines of the form: " 0080: 00 5a 4b ..."
		if ($line !~ /^[0-9a-f]+:/i) {
			next;
		}
		# Cut textual representation at end of line away.
		$line =~ s/\s*\ \ \ .*$//g;
		# Cut everything before first hex digit and after last away.
		$line =~ s/^.*:\ \ |\s*$//g;
		# Convert to binary.
		$line =~ s/\s*([0-9a-f]{2})\s*/chr(hex($1))/ge;
		$blob .= $line;
	}
	return $blob;
}


