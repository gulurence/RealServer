#!/usr/bin/perl
# Clean garbled Chinese from source files
use strict;
use warnings;

my %replacements = (
    # xTime.h/cpp
    '閺傛湹鑵戦悧鍫濈秹閿涘本鍨? => '获取当前微秒时间戳',
    '閺傛湹鑵戦悧鍫濈穱閿? => '获取当前秒时间戳',
    '閸︽澘娼╅弴鎰畱閿涘奔绶? => '设置微秒时间戳',
    '閸︽澘娼╅弴鎰础閿涘奔绶? => '设置秒时间戳',
    '閹笛嗩攽閻€劍鍩€閿涳拷 => '开始计时',
    '閹笛嗩攽閿涘本鍨ч幑顫礆 => '经过的秒数',
    '閹笛嗩攽閿涘本鍨ч崠鏍ㄦ殌鐎? => '经过的毫秒数',
    '閹笛嗩攽閿涘本鍨ч崠鏍ф殌鐎? => '经过的微秒数',

    # xObjPool.h
    '娣囨繂鐡ㄧ粚娲＝閻ㄥ嫯濡悙褰掓Щ濮濄垹顦垮▎鈥崇秺鏉? => '对象池模板 — 预分配对象，避免频繁 new/delete',
    '閺冄呮畱閺佺増宓佺拋鍓х枂鏉╁洦娼?' => '预分配指定数量的对象',
    '閺傛壆鏁电拠椋庢畱閺佷即鍣?' => '销毁所有对象并释放内存',
    '闁插秷娴嘥?new' => '从池中分配新对象（类似 new）',
    'delete' => '将对象归还池中（类似 delete）',
    '閼哄倻鍋ｅ鑼病閸︺劎鈹栭梻鎻掑灙鐞涖劋鑵戞禍?' => '动态扩容 — 池满时自动增长',

    # xExecutionTime.h
    '缂佺姭鍋撻柛妤佹礈濞堟垿骞嶈椤㈡垿寮崼鏇燂紵婵炴潙顑堥惁顖炲礄閼恒儲娈?' => '执行时间测试类 — 用于性能分析和调试',
    '婵炲棌鍓濋弳?' => '执行次数',
    '闁诡剚妲掗埀顒侇殕濡?' => '总耗时（微秒）',

    # xSynlock.h
    '閺嶇厧绱￠崥搴ｇ礉缁?' => '临界区锁 — Windows CRITICAL_SECTION / Linux pthread_mutex',
    '閻戣姤鏆熼弮鍫曟？' => '互斥锁 — Windows Mutex / Linux pthread_mutex',
    '閻㈠疇顔曟径' => 'RAII 作用域锁 — 构造加锁，析构解锁',

    # xCommand headers
    '闁谎嗩嚙缂嶅秹骞愰崶锔藉Б' => '登录命令协议定义',
    '闁谎冾煼濡句即寮? => '登录数据：',
    '闁告帞濞€濞呭海鎲撮幒鏇烆棌' => '创建角色',
    '闂佹彃绉撮幃?' => '错误码：创建角色',
    '闁告帗绋戠紓鎾舵喆閹烘洖顥?' => '角色列表',
    '闁告艾绉惰ⅷ' => '角色名称',
    '闁煎崬濂旂粭?' => '职业',

    # xStringTool.cpp
    'ERROR閿涗礁婀弽鐓庣础閸栨牕鐡х粭锔胯' => 'ERROR: 字符串格式化失败',

    # Generic one-offs
    '闁?' => '',
);

my @files = @ARGV;
foreach my $file (@files) {
    open(my $fh, '<:utf8', $file) or next;
    my $content = do { local $/; <$fh> };
    close($fh);

    my $changed = 0;

    # Apply specific replacements
    while (my ($garbled, $clean) = each %replacements) {
        if ($content =~ /\Q$garbled\E/) {
            $content =~ s/\Q$garbled\E/$clean/g;
            $changed = 1;
        }
    }

    # Generic: replace any remaining garbled CJK chars in comments with clean placeholder
    $content =~ s{//([^\n]*[\x{5800}-\x{9fff}][^\n]*)}{"// " . clean_comment($1)}ge;

    if ($changed || $content =~ /[\x{5800}-\x{9fff}]/) {
        # Write back
        open(my $out, '>:utf8', $file) or next;
        print $out $content;
        close($out);
        print "Fixed: $file\n";
    }
}

sub clean_comment {
    my $s = shift;
    # Remove garbled chars
    $s =~ s/[\x{5800}-\x{9fff}\x{4000}-\x{57ff}]//g;
    $s =~ s/[\?\?]{2,}//g;
    $s =~ s/\s+/ /g;
    $s =~ s/^\s+//;
    $s =~ s/\s+$//;
    return $s;
}

print "Done.\n";
