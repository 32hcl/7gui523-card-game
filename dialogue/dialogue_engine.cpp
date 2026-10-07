#include "dialogue_engine.h"

// ── 辅助宏 ──
#define BOSS(text)  { QString(), QStringLiteral(text), true }
#define NARR(text)  { QString(), QStringLiteral(text), false }

DialogueEngine& DialogueEngine::instance()
{
    static DialogueEngine s_instance;
    return s_instance;
}

DialogueEngine::DialogueEngine()
{
    initData();
}

QVector<DialogueLine> DialogueEngine::getDialogue(int level, DialogueTrigger trigger) const
{
    QString key = QString::number(level) + "_" + QString::number(static_cast<int>(trigger));
    return m_dialogues.value(key);
}

QString DialogueEngine::getBossIntro(int level) const
{
    return m_bossIntros.value(level);
}

QString DialogueEngine::getBossTitle(int level) const
{
    return m_bossTitles.value(level);
}

QString DialogueEngine::getLevelDescription(int level) const
{
    return m_levelDescs.value(level);
}

void DialogueEngine::initData()
{
    // ════════════════════════════════════════════════
    //  第1关：新手 — 赌场门口的小伙计
    // ════════════════════════════════════════════════
    {
        const int L = 1;
        m_bossTitles[L] = "赌场学徒";
        m_bossIntros[L] = "鸿运赌场的入门学徒，刚学会打牌规则。\n作为你的第一个对手，他连牌都还没摸熟。";
        m_levelDescs[L] = "赌场大门敞开，金漆招牌闪得你睁不开眼。\n门口一个年轻学徒正擦着牌桌，见你进来，连忙站直了身子。";

        QString preKey = QString("%1_%2").arg(L).arg(static_cast<int>(DialogueTrigger::PreBattle));
        m_dialogues[preKey] = {
            NARR("赌场门口，一个穿着不合身马甲的年轻学徒正在擦牌桌。"),
            BOSS("啊！有客人来了！我、我是这里的学徒，叫我新手就好。"),
            BOSS("老板说……说要我先陪客人练练手……我刚学的，你、你手下留情啊！"),
            NARR("他紧张得手心冒汗，牌都拿不稳。"),
        };

        QString winKey = QString("%1_%2").arg(L).arg(static_cast<int>(DialogueTrigger::OnPlayerWin));
        m_dialogues[winKey] = {
            BOSS("输、输了……我就知道我不行……"),
            BOSS("你好厉害啊！能不能教教我怎么出牌？"),
            NARR("新手一脸崇拜地看着你。看来这赌场也不是人人都是高手。"),
        };

        QString loseKey = QString("%1_%2").arg(L).arg(static_cast<int>(DialogueTrigger::OnBossWin));
        m_dialogues[loseKey] = {
            BOSS("诶？我、我赢了？！"),
            BOSS("哈哈哈我赢了！老板我赢了一局！我可以转正了吗！"),
            NARR("新手兴奋得跳了起来，完全没注意到你黑着的脸。"),
        };

        QString bombKey = QString("%1_%2").arg(L).arg(static_cast<int>(DialogueTrigger::OnFirstBomb));
        m_dialogues[bombKey] = {
            BOSS("哇！炸、炸弹！好厉害！"),
            BOSS("我只在书上见过……原来炸弹长这样！"),
        };
    }

    // ════════════════════════════════════════════════
    //  第2关：换牌虫 — 出老千的小混混
    // ════════════════════════════════════════════════
    {
        const int L = 2;
        m_bossTitles[L] = "街头老千";
        m_bossIntros[L] = "在赌场边缘混饭吃的小老千。\n手法不算高明，但偷偷换牌的动作很熟练。";
        m_levelDescs[L] = "赌场侧厅的一张小牌桌，一个獐头鼠目的男人正\n贼眉鼠眼地打量着四周，袖子里似乎藏着什么。";

        QString preKey = QString("%1_%2").arg(L).arg(static_cast<int>(DialogueTrigger::PreBattle));
        m_dialogues[preKey] = {
            NARR("侧厅的牌桌旁，一个瘦男人正用指尖转着扑克牌。"),
            BOSS("哟，新来的？看你面生啊。"),
            BOSS("我叫换牌虫——别问我为什么叫这名儿，打牌就打牌，话那么多。"),
            NARR("他说话的时候，左手一直在袖子里摸索着什么……"),
        };

        QString winKey = QString("%1_%2").arg(L).arg(static_cast<int>(DialogueTrigger::OnPlayerWin));
        m_dialogues[winKey] = {
            BOSS("不可能……我明明换了牌的！"),
            BOSS("等等——你没看到我换牌？不可能啊，我动作很快的！"),
            NARR("他慌慌张张地检查袖子，发现里面的牌早就不对了。"),
        };

        QString loseKey = QString("%1_%2").arg(L).arg(static_cast<int>(DialogueTrigger::OnBossWin));
        m_dialogues[loseKey] = {
            BOSS("嘿嘿，怎么样，输了吧？"),
            BOSS("告诉你，在这赌场混，不会点手段可不行~"),
            NARR("他得意地晃了晃袖子，几张牌从里面掉了出来。"),
        };

        QString bombKey = QString("%1_%2").arg(L).arg(static_cast<int>(DialogueTrigger::OnFirstBomb));
        m_dialogues[bombKey] = {
            BOSS("切，炸弹了不起啊……"),
            BOSS("（小声）等会儿我也换一副出来……"),
        };
    }

    // ════════════════════════════════════════════════
    //  第3关：急眼雀 — 输不起的爆脾气赌客
    // ════════════════════════════════════════════════
    {
        const int L = 3;
        m_bossTitles[L] = "暴躁赌客";
        m_bossIntros[L] = "赌场常客，脾气比牌技大得多。\n据说他输了会掀桌子，所以被安排在最角落的位置。";
        m_levelDescs[L] = "赌场角落的一张牌桌，地上散落着几张被折坏的牌。\n一个满脸横肉的男人正烦躁地敲着桌子。";

        QString preKey = QString("%1_%2").arg(L).arg(static_cast<int>(DialogueTrigger::PreBattle));
        m_dialogues[preKey] = {
            NARR("你刚坐下，对面的男人就不耐烦地敲了敲桌子。"),
            BOSS("磨磨蹭蹭的搞什么！赶紧的！"),
            BOSS("老子叫急眼雀，在这赌场没人不知道！快出牌，别浪费我时间！"),
            NARR("他气势汹汹，但你注意到——他是先手。"),
        };

        QString winKey = QString("%1_%2").arg(L).arg(static_cast<int>(DialogueTrigger::OnPlayerWin));
        m_dialogues[winKey] = {
            BOSS("什么？！我输了？！不可能！"),
            BOSS("你出千！你肯定出千了！老子怎么可能输！"),
            NARR("他猛地站起来想掀桌子，但被旁边的赌场保安按住了。"),
        };

        QString loseKey = QString("%1_%2").arg(L).arg(static_cast<int>(DialogueTrigger::OnBossWin));
        m_dialogues[loseKey] = {
            BOSS("哈哈哈！看到没！老子赢了！"),
            BOSS("就你这水平还来赌场？回家再练十年吧！"),
            NARR("他嚣张得不行，嘴里的话比出牌还快。"),
        };

        QString passKey = QString("%1_%2").arg(L).arg(static_cast<int>(DialogueTrigger::OnPlayerPass));
        m_dialogues[passKey] = {
            BOSS("哈！不要了吧！就知道你不行！"),
        };
    }

    // ════════════════════════════════════════════════
    //  第4关：疯狗 — 赌场看场打手
    // ════════════════════════════════════════════════
    {
        const int L = 4;
        m_bossTitles[L] = "赌场打手";
        m_bossIntros[L] = "赌场的看场打手，人如其名，出起牌来像疯狗咬人。\n每出一张牌都像是在发泄——小心别被咬到。";
        m_levelDescs[L] = "走廊尽头的一扇门里传来咆哮声。\n推开门，一个肌肉虬结的男人正把牌捏得咯吱响。";

        QString preKey = QString("%1_%2").arg(L).arg(static_cast<int>(DialogueTrigger::PreBattle));
        m_dialogues[preKey] = {
            NARR("房间里灯光昏暗，一个壮硕的男人正用拳头砸着牌桌。"),
            BOSS("汪汪汪——！哦不对，咳咳……"),
            BOSS("老子叫疯狗！赌场的规矩都是老子说了算！"),
            BOSS("打牌？老子不会打什么牌——老子只会咬！汪！"),
            NARR("你注意到他的牌上全是牙印。"),
        };

        QString winKey = QString("%1_%2").arg(L).arg(static_cast<int>(DialogueTrigger::OnPlayerWin));
        m_dialogues[winKey] = {
            BOSS("呜……你居然赢了疯狗……"),
            BOSS("汪……汪呜呜……"),
            NARR("疯狗夹着尾巴灰溜溜地跑了。"),
        };

        QString loseKey = QString("%1_%2").arg(L).arg(static_cast<int>(DialogueTrigger::OnBossWin));
        m_dialogues[loseKey] = {
            BOSS("汪！汪汪汪！！"),
            BOSS("看到没！被疯狗咬到了吧！汪汪汪！"),
            NARR("他兴奋地围着牌桌转了三圈，活脱脱一条大狗。"),
        };

        QString bombKey = QString("%1_%2").arg(L).arg(static_cast<int>(DialogueTrigger::OnFirstBomb));
        m_dialogues[bombKey] = {
            BOSS("汪汪！炸弹？！老子最喜欢炸弹了！"),
            BOSS("汪呜呜呜——咬你！"),
        };
    }

    // ════════════════════════════════════════════════
    //  第5关：赖账鬼 — 放高利贷的
    // ════════════════════════════════════════════════
    {
        const int L = 5;
        m_bossTitles[L] = "高利贷主";
        m_bossIntros[L] = "赌场里放高利贷的，最擅长赖账和钻规则空子。\n他说过的话能不算数就不算数，他定的规矩你想都别想改。";
        m_levelDescs[L] = "赌场VIP室的门口，一个穿着讲究但眼神阴鸷的男人\n正用金算盘拨拉着什么，桌上堆着厚厚的欠条。";

        QString preKey = QString("%1_%2").arg(L).arg(static_cast<int>(DialogueTrigger::PreBattle));
        m_dialogues[preKey] = {
            NARR("VIP室里烟雾缭绕，一个男人正用金算盘拨拉着。"),
            BOSS("哟，来挑战我了？先把规矩说清楚——"),
            BOSS("这牌桌上啊，我出的牌你不能用同样的点数压。懂？"),
            BOSS("至于我嘛……嘿嘿，同花的单张，我想怎么压就怎么压。"),
            NARR("他咧嘴一笑，露出一嘴黄牙。这赌场，果然是越往里面越不讲理。"),
        };

        QString winKey = QString("%1_%2").arg(L).arg(static_cast<int>(DialogueTrigger::OnPlayerWin));
        m_dialogues[winKey] = {
            BOSS("……不可能。我定的规矩，怎么可能输？"),
            BOSS("等等……你这牌有问题！我要再看一眼！"),
            NARR("他想耍赖，但你早把牌收好了。赖账鬼第一次赖不掉账。"),
        };

        QString loseKey = QString("%1_%2").arg(L).arg(static_cast<int>(DialogueTrigger::OnBossWin));
        m_dialogues[loseKey] = {
            BOSS("哈哈哈，输了吧？早就跟你说了，规矩是我定的。"),
            BOSS("来来来，把筹码拿过来。利息嘛……就按九出十三归算。"),
            NARR("他笑得像只偷到油的老鼠。"),
        };

        QString rocketKey = QString("%1_%2").arg(L).arg(static_cast<int>(DialogueTrigger::OnFirstRocket));
        m_dialogues[rocketKey] = {
            BOSS("王、王炸？！"),
            BOSS("……这、这个不算！王炸不能压我！我说了算！"),
        };
    }

    // ════════════════════════════════════════════════
    //  第6关：不识数 — 装傻的荷官
    // ════════════════════════════════════════════════
    {
        const int L = 6;
        m_bossTitles[L] = "赌场荷官";
        m_bossIntros[L] = "看起来傻乎乎的赌场荷官，总是数不清牌。\n但你总觉得……他是不是装的？";
        m_levelDescs[L] = "主厅中央的一张大赌桌，一个戴着滑稽眼镜的荷官\n正歪着头数牌，数了三遍还是数错。";

        QString preKey = QString("%1_%2").arg(L).arg(static_cast<int>(DialogueTrigger::PreBattle));
        m_dialogues[preKey] = {
            NARR("主厅的大赌桌前，一个戴着厚眼镜的荷官正在数牌。"),
            BOSS("一、二、三……五、七、九……诶？不对不对……"),
            BOSS("啊，客人来了！我叫不识数，是这里的荷官。"),
            BOSS("那、那个……我们开始吧？我数牌可能有点慢，你别介意啊嘿嘿。"),
            NARR("他傻笑着，但你总觉得他的眼神藏在镜片后面，看不清。"),
        };

        QString winKey = QString("%1_%2").arg(L).arg(static_cast<int>(DialogueTrigger::OnPlayerWin));
        m_dialogues[winKey] = {
            BOSS("啊？输了？我输了吗？"),
            BOSS("等、等一下让我数数……一、二、三……哦，好像是输了嘿嘿。"),
            NARR("他挠着头傻笑，但你注意到——他的牌摆得整整齐齐，一张不多一张不少。"),
        };

        QString loseKey = QString("%1_%2").arg(L).arg(static_cast<int>(DialogueTrigger::OnBossWin));
        m_dialogues[loseKey] = {
            BOSS("诶？我赢了吗？真的吗？"),
            BOSS("嘿嘿……好像是我赢了呢。我也不知道怎么赢的，运气好运气好~"),
            NARR("他推了推眼镜，镜片闪过一道光。这人……真的不识数吗？"),
        };

        QString specialKey = QString("%1_%2").arg(L).arg(static_cast<int>(DialogueTrigger::OnSpecial523));
        m_dialogues[specialKey] = {
            BOSS("……七鬼五二三？"),
            BOSS("……有意思。"),
            NARR("他的笑容消失了一瞬，但很快又恢复了那副傻乎乎的样子。"),
        };
    }

    // ════════════════════════════════════════════════
    //  第7关：黑老大 — 赌场暴力头目
    // ════════════════════════════════════════════════
    {
        const int L = 7;
        m_bossTitles[L] = "赌场头目";
        m_bossIntros[L] = "赌场的暴力头目，手下管着疯狗和一众打手。\n据说他从不用出千，只靠气势就能把对手压垮。";
        m_levelDescs[L] = "赌场地下室的铁门后，一个纹着青龙的壮汉\n正用匕首钉着扑克牌，每一下都钉进木头三分。";

        QString preKey = QString("%1_%2").arg(L).arg(static_cast<int>(DialogueTrigger::PreBattle));
        m_dialogues[preKey] = {
            NARR("地下室里弥漫着血腥味，一个壮汉正用匕首往墙上钉牌。"),
            BOSS("哼，又来一个找死的。"),
            BOSS("老子是黑老大，这赌场地下的事，都是老子说了算。"),
            BOSS("疯狗那小子是我手下，你能打赢他，有点能耐。"),
            BOSS("不过——跟老子比，你还差得远呢。"),
            NARR("他拔出匕首，刀尖上挑着一张黑桃A。"),
        };

        QString winKey = QString("%1_%2").arg(L).arg(static_cast<int>(DialogueTrigger::OnPlayerWin));
        m_dialogues[winKey] = {
            BOSS("……不可能。老子居然输了？"),
            BOSS("行，你有种。上面的千里眼在等你。"),
            BOSS("那婆娘眼睛毒得很，你小心点。"),
            NARR("黑老大把匕首往桌上一插，头也不回地走了。"),
        };

        QString loseKey = QString("%1_%2").arg(L).arg(static_cast<int>(DialogueTrigger::OnBossWin));
        m_dialogues[loseKey] = {
            BOSS("哈哈哈！就这点本事？"),
            BOSS("疯狗那小子没教你吗？在赌场，拳头比牌管用！"),
            NARR("他一巴掌拍在桌上，牌都震飞了。"),
        };

        QString bombKey = QString("%1_%2").arg(L).arg(static_cast<int>(DialogueTrigger::OnFirstBomb));
        m_dialogues[bombKey] = {
            BOSS("炸弹？哼，老子见多了。"),
            BOSS("有本事你再炸一个试试！"),
        };
    }

    // ════════════════════════════════════════════════
    //  第8关：千里眼 — 赌场监察
    // ════════════════════════════════════════════════
    {
        const int L = 8;
        m_bossTitles[L] = "赌场监察";
        m_bossIntros[L] = "赌场的监察头目，传说她能看穿每一张牌的背面。\n没有人能在她眼皮底下出千——也没有人能赢她。";
        m_levelDescs[L] = "监控室里，数百个屏幕闪着光。\n一个蒙着面纱的女子背对着你，指尖在屏幕上飞速划过。";

        QString preKey = QString("%1_%2").arg(L).arg(static_cast<int>(DialogueTrigger::PreBattle));
        m_dialogues[preKey] = {
            NARR("监控室里，数百个屏幕同时闪烁，像无数双眼睛。"),
            BOSS("来了。"),
            BOSS("我是千里眼。这赌场里的每一张牌，我都看得清清楚楚。"),
            BOSS("黑老大居然输给你了……有意思。"),
            BOSS("不过你别得意。在我面前，你手里的每一张牌我都知道。"),
            NARR("她转过身，面纱下的眼睛仿佛能看透一切。"),
        };

        QString winKey = QString("%1_%2").arg(L).arg(static_cast<int>(DialogueTrigger::OnPlayerWin));
        m_dialogues[winKey] = {
            BOSS("……我居然看错了？"),
            BOSS("不可能……我的眼睛从来不会错……"),
            BOSS("……七鬼五二三？原来如此。"),
            BOSS("去吧，最顶层的老板在等你。"),
            BOSS("但我提醒你——那扇门后面，没有赢家。"),
            NARR("她的声音里第一次有了波动。"),
        };

        QString loseKey = QString("%1_%2").arg(L).arg(static_cast<int>(DialogueTrigger::OnBossWin));
        m_dialogues[loseKey] = {
            BOSS("看到了吗？你的牌，我一张都没看错。"),
            BOSS("在我千里眼面前，没有秘密。"),
            NARR("她挥了挥手，屏幕上一一显示出你刚才出过的每一张牌，丝毫不差。"),
        };

        QString specialKey = QString("%1_%2").arg(L).arg(static_cast<int>(DialogueTrigger::OnSpecial523));
        m_dialogues[specialKey] = {
            BOSS("！"),
            BOSS("七鬼五二三……我居然没看出来……"),
        };
    }

    // ════════════════════════════════════════════════
    //  第9关：赌场老板 — 最终BOSS
    // ════════════════════════════════════════════════
    {
        const int L = 9;
        m_bossTitles[L] = "赌场老板";
        m_bossIntros[L] = "鸿运赌场的创始人，神秘的幕后老板。\n没有人见过他的真面目，只知道他从开赌场那天起就没输过。";
        m_levelDescs[L] = "顶层办公室，整面墙都是落地窗，能看到整座城市。\n一个背对着你的身影站在落地窗前，手里转着一枚筹码。";

        QString preKey = QString("%1_%2").arg(L).arg(static_cast<int>(DialogueTrigger::PreBattle));
        m_dialogues[preKey] = {
            NARR("顶层办公室，整面墙都是落地窗。城市的夜景在脚下铺开。"),
            NARR("一个背对着你的身影站在窗前，手里转着一枚金色筹码。"),
            BOSS("你来了。"),
            BOSS("从一楼一路打到九楼，不容易。"),
            BOSS("我是这家赌场的老板。他们都叫我——"),
            BOSS("算了，名字不重要。重要的是——"),
            BOSS("你知道七鬼五二三的真正含义吗？"),
            NARR("他转过身来。你看不清他的脸，因为他站在逆光里。"),
        };

        QString winKey = QString("%1_%2").arg(L).arg(static_cast<int>(DialogueTrigger::OnPlayerWin));
        m_dialogues[winKey] = {
            NARR("最后一张牌落下，窗外的城市灯火仿佛都静止了。"),
            BOSS("……你赢了。"),
            BOSS("三十年了，你是第一个赢我的人。"),
            NARR("他缓缓转过身。你终于看清了他的脸——"),
            NARR("那是一张你无比熟悉的脸。"),
            BOSS("这家赌场，从今天起，是你的了。"),
            BOSS("但记住一句话——"),
            BOSS("凡是在赌桌上赢来的，终将在赌桌上输掉。"),
            BOSS("七鬼五二三……不过是一场梦。"),
            NARR("说完，他转身走向落地窗，一步踏了出去。"),
            NARR("你冲过去看——楼下空空如也，只有一张牌缓缓飘落。"),
            NARR("牌面上，写着一个字：「醒」。"),
        };

        QString loseKey = QString("%1_%2").arg(L).arg(static_cast<int>(DialogueTrigger::OnBossWin));
        m_dialogues[loseKey] = {
            BOSS("输了。"),
            BOSS("那就留下来吧。"),
            BOSS("这赌场里，永远缺一个好对手。"),
            NARR("落地窗上，你的倒影渐渐模糊，与窗外的夜色融为一体。"),
            NARR("—— 赌场永远不打烊，而你，成为了新的荷官。 ——"),
        };

        QString specialKey = QString("%1_%2").arg(L).arg(static_cast<int>(DialogueTrigger::OnSpecial523));
        m_dialogues[specialKey] = {
            BOSS("……！"),
            BOSS("七、鬼、五、二、三……你居然凑齐了。"),
            BOSS("原来如此。原来你就是……"),
            NARR("他第一次正面看向你。逆光中，你似乎看到他笑了。"),
        };
    }
}
