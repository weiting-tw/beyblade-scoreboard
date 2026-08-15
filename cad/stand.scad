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
// "stand" = 完整立座；"test_ring" = 只印承接環，用來驗證直徑配合
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
base_t       = 3.0;   // 底板厚度
vent_dia     = 0;     // 背板中央開孔直徑；0 = 不開

/* [外壁開口] */
// "arcs" = 只保留三段支撐弧，上半部全開（安全預設：鋁殼側面的
//          PWR / BOOT / USB-C 角向位置未實測，整圈壁可能壓到按鈕）
// "full" = 完整外圈，再依 port_cuts 逐一開槽（量完角度後改用這個）
rim_mode = "arcs";

// 角度慣例：0 = 螢幕正上方（12 點鐘），順時針為正。
// [中心角度, 開口跨度]
arc_gaps = [
    [   0, 140],  // 上半部全開，取放也方便
    [ 128,  35],  // 右側
    [ 232,  35],  // 左側
];

// rim_mode = "full" 時使用。角度是待實測的佔位值，不要直接照印。
port_cuts = [
    [ 180, 22],  // USB-C
    [ 135, 16],  // PWR
    [ 225, 16],  // BOOT / RESET
];

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
    if (vent_dia > 0)
        translate([0, 0, -1]) cylinder(h = floor_t + 2, d = vent_dia);
}

module gap_cuts() {
    gaps = (rim_mode == "full") ? port_cuts : arc_gaps;
    for (g = gaps)
        translate([0, 0, floor_t])
            pie(g[0], g[1], outer_dia, recess_depth + 1);
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

module stand() {
    difference() {
        union() {
            placed() disc_solid();
            foot();
        }
        // 這三個切除放在最外層，foot 的凸包填進凹槽的部分才會一併挖掉
        placed() recess_cut();
        placed() vent_cut();
        placed() gap_cuts();
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

if (mode == "test_ring") test_ring();
else                     stand();
