#!/usr/bin/perl -w
@txtlist = ();
@tsdir = ();
if (! scalar (@ARGV)) {
	if (! -d "./comm") {
		print "Didn't find 'comm' directory\n";
		exit(1);
	}
	if (! -d "./timestamp") {
		print "Didn't find 'timestamp' directory\n";
		exit(1);
	}
	foreach $dir (glob "./comm/*") {
		($root) = ($dir =~ /([^\/\\]+)$/);
		($txt) = glob "$dir/*.txt";
		if (! defined $txt) {
			printf "Couldn't find $dir/*.txt\n";
		} elsif (-d $dir && -f $txt && -d "./timestamp/$root") {
			print "Found $dir\n";
			push @txtlist, $txt;
			push @tsdir, "./timestamp/$root";
		}
	}
} else {
	if ($ARGV[0] =~ /^(.*).txt$/ && -e $ARGV[0]) {
  		push @txtlist, $ARGV[0];
	} else {
		print "Didn't find file: $ARGV[0]\n";
		exit (1);
	}
	if (! -d $ARGV[1]) {
		print "Couldn't find timestamp directory: $ts_dir\n";
		exit (1);
	} else {
		push @tsdir, $ARGV[1];
	}
}
while (@txtlist) {
	$txt_file = pop @txtlist;
	$ts_dir = pop @tsdir;
	$ts_merge_file = $txt_file;
	$ts_merge_file =~ s/\.txt$/.ts/;
	print "building $ts_merge_file from $txt_file and $ts_dir\n";
open (FH, $txt_file);
open (tsFH, ">$ts_merge_file");
while (<FH>) {
	if (/^#\((\S+)\)/) {
		$name = $1;
		$str = "";
		if (/\s+(\S+)\s*$/) {
			$voice_file = $1;
			($ts_file) = ($voice_file =~ /^(.*).ogg/);
			$ts_file = "$ts_dir/$ts_file.ts";
			if (-f $ts_file) {
				open (ts1FH, $ts_file);
				@data=();
				$chksum = 0;
				print "$ts_file\n";
				while (<ts1FH>) {
					chomp;
					if (/Checksum:\s+(\d+)/) {
						$chksum = $1;
					}
					if (/^\s+(\S+)/) {
						$value = int ($1 *1000);
						if ($value != 0) {
							push @data, $value - $last_val;
						}
						$last_val = $value;
					}
				}
				close (ts1FH);
				$str = join ",", @data;
#				print "$chksum: $str\n";
			}
		}
		print tsFH "#($name) $str\n";
	}
}
close (FH);
close tsFH;
}
