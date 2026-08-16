// Beyblade X 計分器 —— 傾斜桌面立座
//
// 承接對象是「含現成鋁殼的完整本體」，不是裸 PCB，所以不需要對 PCB 孔位，
// 只需要一個直徑與厚度都對得上的圓形凹槽。
//
// 匯出 STL：
//   openscad -o stand.stl cad/stand.scad
//   openscad -o test_ring.stl -D 'mode="test_ring"' cad/stand.scad
//
// 建議流程：先印 test_ring（薄片，約十分鐘）確認 Ø 配合鬆緊，再印完整立座。

/* [模式] */
// "stand"     = 完整立座
// "test_ring" = 只印承接環，驗證直徑配合與弧度
// "cal_ring"  = 校正環：承接環外圈加寬並刻上角度刻度，用來量出開孔的實際
//               角向位置。把本體壓進去，看它的開孔中心對到哪一格。
mode = "stand";

/* [實測值 —— 改這裡] */
// 含鋁殼本體的外徑。圓形物體用游標卡尺要轉幾個角度取最大值，量到弦會偏小。
body_dia = 61.0;
// 含鋁殼本體的厚度
body_thick = 11.0;

/* [配合公差] */
// 單邊間隙。FDM 建議 0.3~0.4；太緊塞不進、太鬆會晃。
// 實際凹槽直徑 = body_dia + 2 * fit_clearance
fit_clearance = 0.35;

/* [姿態] */
// 後仰角（度）。0 = 螢幕完全直立，30 = 向後仰 30 度。
lean_back = 30;

/* [結構] */
recess_depth = 4.0;   // 本體嵌入深度。露出 body_thick - recess_depth
floor_t      = 3.0;   // 背板厚度
wall         = 3.0;   // 承接環外壁厚
foot_depth   = 42.0;  // 底座前後深度。後仰越多要越深，否則會後翻。
base_t       = 8.0;   // 底板厚度。要夠高才容得下前緣的棘齒紋
// 背板中央開孔直徑。0 = 實心平底。
//
// 本體背面若是弧的（實測就是），平底會變成中間頂住、邊緣浮空 —— 板子放不
// 到底、還會晃。中央挖空之後弧形凸起有地方去，只靠外圈一圈承接，完全不必
// 知道曲率是多少。46 時承接環寬 (61.7-46)/2 = 7.85mm，撐得住。
vent_dia     = 46;

/* [外壁開口] */
// "arcs" = 只保留三段支撐弧，上半部全開（不知道開孔位置時的安全選擇）
// "full" = 完整外圈，再依 port_cuts 逐一開槽（開孔位置已實測，用這個）
rim_mode = "full";

// 角度慣例：0 = 螢幕正上方（12 點鐘），順時針為正。
// [中心角度, 開口跨度]
arc_gaps = [
    [   0, 140],  // 上半部全開，取放也方便
    [ 128,  35],  // 右側
    [ 232,  35],  // 左側
];

// rim_mode = "full" 時使用。[中心角度, 開孔實測寬度 mm] —— 寫 mm 而不是角度，
// 因為卡尺量出來就是 mm，換算交給程式。
//
// 實測值（螢幕正上方 0°，順時針為正）：
port_cuts = [
    // 按鈕與 USB-C 合併成一個開口。兩孔中心只差 45 度（約 24mm），扣掉各自
    // 半寬後中間那道壁只剩 1.45mm —— 0.4mm 噴嘴約三條線，細到容易斷。
    // 而且充電指示 LED 就在這一區（約 70 度），整片開放正好讓它露出來。
    [  68, 45],  // BOOT/RESET 按鈕 + USB-C + 充電 LED
    [ 135, 10],  // PWR
    [ 180, 15],  // 喇叭出音孔 —— 不開的話聲音會被悶住
    [ 270, 15],  // SD 卡
];

// 開槽比開孔每側多讓這麼多，讓插頭與手指進得去。
//
// 上限就是 1.0：按鈕與 USB-C 中心只差 45°（約 24mm），扣掉各自半寬只剩
// 3.45mm，每側再讓 2mm 兩個槽就會重疊、中間那道壁直接消失。
// 目前 1.0 之下該處壁厚 1.45mm（0.4mm 噴嘴約 3.6 條線），印得出來但偏細，
// 斷掉的話把這兩筆合併成一個 [67, 45] 的槽即可。
port_margin = 1.0;

/* [棘齒紋] */
// 底座前緣下方切一排 V 形槽，取 Beyblade 棘輪的意象。
// 刻意用「切」而不是「加」—— 凸出來的齒 FDM 要架支撐，切進去的槽不用，
// 而且不影響底座強度分布。
ratchet = true;
ratchet_count = 5;    // 每側齒數。齒間留壁要 >1mm，太密印不出來
ratchet_depth = 2.0;  // 切入深度
ratchet_h = 6.0;      // 齒紋帶高度，必須小於 base_t
// 方柱中心要放在側面平面「外側」這麼多。中心剛好落在平面上的話，方柱的角
// 會與表面零厚度接觸，切出來的 STL 帶一堆非流形邊。
ratchet_out = 1.0;

/* [校正環] */
// 外緣加寬到這個直徑放刻度。承接環本身的壁只有 3mm，刻度擠在上面讀不出來。
cal_outer_dia = 96;
cal_thick = 3;      // 刻度盤厚度
cal_tick_depth = 1.0;

/* [其他] */
$fn = 120;

// ---------------------------------------------------------------- 推導值

recess_dia = body_dia + 2 * fit_clearance;
outer_dia  = recess_dia + 2 * wall;
plate_h    = floor_t + recess_depth;

// 承接環繞 X 軸旋轉的角度：90 = 完全直立
tilt = 90 - lean_back;

// 旋轉後把整體抬到剛好接觸 z = 0
lift = (outer_dia / 2) * sin(tilt);

// 底座往後偏移，讓支撐面涵蓋後仰造成的重心位移
foot_offset_y = foot_depth * 0.18;

// hull 到底板時只取承接環最下面這一段，避免凸包吃掉整個造型
foot_blend_h = 8.0;

big = 400;  // 「夠大」的切除體尺寸

// ---------------------------------------------------------------- 基本元件

// 扇形柱體，用來切外壁開口。a_mid 為中心角度（12 點鐘起算，順時針）。
module pie(a_mid, span, r, h) {
    // 轉成 OpenSCAD 極角：0 度在 +X，逆時針為正
    p0 = 90 - a_mid - span / 2;
    seg = max(8, ceil(span / 5));
    linear_extrude(h)
        polygon(concat(
            [[0, 0]],
            [for (i = [0 : seg]) let (a = p0 + span * i / seg)
                [r * cos(a), r * sin(a)]]
        ));
}

// 承接環的局部座標：XY 平面，+Z 為螢幕朝向，+Y 為裝好後的上方
module disc_solid() {
    cylinder(h = plate_h, d = outer_dia);
}

// 凹槽往 +Z 延伸得夠遠，確保本體放入的路徑不會被底座材料擋住
module recess_cut() {
    translate([0, 0, floor_t]) cylinder(h = big, d = recess_dia);
}

module vent_cut() {
    // 往背面多挖 15mm，不是只挖穿 3mm 的背板 —— 背板後面就是 foot 的凸包
    // 材料，只挖穿背板的話弧形凸起照樣頂到底座，等於沒挖。
    if (vent_dia > 0)
        translate([0, 0, -15]) cylinder(h = floor_t + 16, d = vent_dia);
}

module gap_cuts() {
    if (rim_mode == "full") {
        // 開孔在本體側面，所以用 body_dia 的周長換算 mm -> 角度。
        circumference = PI * body_dia;
        for (p = port_cuts)
            translate([0, 0, floor_t])
                pie(p[0], (p[1] + 2 * port_margin) / circumference * 360,
                    outer_dia, recess_depth + 1);
    } else {
        for (g = arc_gaps)
            translate([0, 0, floor_t])
                pie(g[0], g[1], outer_dia, recess_depth + 1);
    }
}

// ---------------------------------------------------------------- 擺位

module placed() {
    translate([0, 0, lift]) rotate([tilt, 0, 0]) children();
}

// ---------------------------------------------------------------- 立座

module foot() {
    hull() {
        // 接地的底板
        translate([0, foot_offset_y, 0])
            linear_extrude(base_t)
                offset(r = 4)
                    square([outer_dia - 8, foot_depth - 8], center = true);

        // 承接環最下面一段
        intersection() {
            placed() disc_solid();
            translate([-big / 2, -big / 2, 0]) cube([big, big, foot_blend_h]);
        }
    }
}

// 底座左右側面的 V 形槽。用旋轉 45 度的方柱去切，對角線朝側面，
// 切出來就是等腰三角形的槽口。
//
// 刻意放側面而不是前緣：底板半寬與承接環半徑都是 outer_dia/2，hull 出來的
// 側面是垂直平面，位置不隨高度變，方柱中心擺在平面上就一定是「從外面切進
// 去」。前緣則因為 hull 會往前延伸，固定的 y 會整個埋進材料裡，切出來的是
// 內部空腔而不是槽 —— 那種 STL 是多殼非流形，切片軟體會出錯。
module ratchet_cuts() {
    span = foot_depth * 0.62;
    step = span / ratchet_count;
    // 中心外推 ratchet_out，對角線同步加大，最深處仍切入 ratchet_depth
    edge = (ratchet_depth + ratchet_out) * 1.414;
    for (side = [-1, 1]) {
        for (i = [0 : ratchet_count - 1]) {
            translate([side * (outer_dia / 2 + ratchet_out),
                       foot_offset_y - span / 2 + step * (i + 0.5),
                       1.0 + ratchet_h / 2])
                rotate([0, 0, 45])
                    cube([edge, edge, ratchet_h], center = true);
        }
    }
}

module stand() {
    difference() {
        union() {
            placed() disc_solid();
            foot();
        }
        if (ratchet) ratchet_cuts();
        // 這三個切除放在最外層，foot 的凸包填進凹槽的部分才會一併挖掉
        placed() recess_cut();
        placed() vent_cut();
        placed() gap_cuts();
    }
}

// 角度刻度盤。0 度在正上方、順時針為正，與 port_cuts 的角度慣例一致。
// 每 10 度一格、每 30 度一長格，0 度那一格特別長並額外加一條，
// 免得數格子時數錯起點。
module cal_ticks() {
    r_out = cal_outer_dia / 2 - 1;
    for (a = [0 : 10 : 359]) {
        major = (a % 30 == 0);
        len = (a == 0) ? 13 : (major ? 9 : 5);
        w = major ? 1.4 : 0.9;
        // 角度慣例轉換：0 度朝 +Y（正上方），順時針為正。
        rotate([0, 0, 90 - a])
            translate([r_out - len / 2, 0, cal_thick - cal_tick_depth / 2])
                cube([len, w, cal_tick_depth + 0.1], center = true);
    }
    // 0 度再加一條短的並排，兩條並排的那邊就是起點。
    rotate([0, 0, 90])
        translate([cal_outer_dia / 2 - 5, 3.5, cal_thick - cal_tick_depth / 2])
            cube([8, 0.9, cal_tick_depth + 0.1], center = true);
}

module cal_ring() {
    difference() {
        union() {
            disc_solid();
            // 刻度盤只做薄薄一層，貼在承接環底部往外延伸。
            cylinder(h = cal_thick, d = cal_outer_dia);
        }
        recess_cut();
        vent_cut();
        gap_cuts();
        cal_ticks();
    }
}

// 驗證片：只有承接環，用來確認直徑配合鬆緊
module test_ring() {
    difference() {
        disc_solid();
        recess_cut();
        vent_cut();
        gap_cuts();
    }
}

// ---------------------------------------------------------------- 輸出

if (mode == "test_ring")     test_ring();
else if (mode == "cal_ring") cal_ring();
else                         stand();
