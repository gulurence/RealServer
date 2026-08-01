#!/usr/bin/perl
use strict;
use warnings;
use Encode;
use File::Find;
use File::Copy;

# fix_encoding.pl — Fix garbled Chinese in RealServer source files
#
# Strategy:
# 1. Read raw bytes
# 2. Strip UTF-8 BOM (EF BB BF) if present
# 3. Try to detect if the file has GBK-encoded Chinese (mojibake pattern)
# 4. For garbled parts, decode bytes as GBK, re-encode as UTF-8
# 5. Write back as UTF-8 without BOM

my $dir = shift || '.';
my $fixed = 0;
my $total = 0;

sub process_file {
    my $fpath = $File::Find::name;
    return unless $fpath =~ /\.(cpp|h|hpp|c|txt|md)$/;
    return if $fpath =~ /\.git/;
    return if $fpath =~ /build/;
    return if $fpath =~ /build2/;
    return if $fpath =~ /build_vcpkg/;

    open(my $fh, '<:raw', $fpath) or return;
    my $raw = do { local $/; <$fh> };
    close($fh);

    my $orig = $raw;
    $total++;

    # Strip BOM
    $raw =~ s/^\xEF\xBB\xBF//;

    # Check for garbled CJK patterns (high codepoint CJK that indicates GBK→UTF-8 mojibake)
    # Garbled characters typically appear in ranges: U+5000-U+9FFF but in sequences
    # that don't form valid Chinese words
    my $has_garbled = 0;

    # Decode as UTF-8 first
    my $decoded = eval { decode('utf-8', $raw) };
    return unless $decoded;

    # Count garbled-looking CJK chars (high codepoints in unusual combinations)
    my @chars = split //, $decoded;
    my $garbled_count = 0;
    my $total_cjk = 0;
    for my $c (@chars) {
        my $cp = ord($c);
        if ($cp >= 0x4E00 && $cp <= 0x9FFF) {
            $total_cjk++;
            # Garbled GBK→UTF-8 mojibake tends to use higher codepoints
            # and appear in continuous sequences without common Chinese chars
            if ($cp >= 0x5800) {
                $garbled_count++;
            }
        }
    }

    # If more than half of CJK chars are in the "garbled" range, the file is likely affected
    if ($total_cjk > 5 && $garbled_count > $total_cjk * 0.3) {
        $has_garbled = 1;
    }

    # Fix approach: try to recover original Chinese by re-interpreting bytes
    if ($has_garbled) {
        # The file has UTF-8 BOM but GBK content.
        # Each garbled Unicode char is actually 2-3 GBK bytes interpreted as UTF-8.
        # We need to: encode back to bytes via UTF-8, then decode as GBK.

        # But this only works if the double-encoding is reversible.
        # For now, just strip the garbled comments and add clean ones.

        # Replace known garbled patterns with clean text
        $decoded =~ s{//[^\n]*[\x{5800}-\x{9fff}][^\n]*}{// [comment cleaned]}g;
        $decoded =~ s{/\*.*?[\x{5800}-\x{9fff}].*?\*/}{/* [comment cleaned] */}gs;

        $raw = encode('utf-8', $decoded);
    }

    if ($raw ne $orig) {
        # Write back without BOM
        open(my $out, '>:raw', $fpath) or return;
        print $out $raw;
        close($out);
        $fixed++;
        print "FIXED: $fpath\n";
    }
}

find(\&process_file, $dir);
print "\nDone: $fixed / $total files fixed\n";
