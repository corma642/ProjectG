"""위젯 시각·입력 회귀 검증 (헤드리스 에디터 Python).

위젯을 실제 RHI로 그려 PNG를 남기고, 버튼이 장식 위젯에 가려지지 않았는지와 런 이벤트 전조 위젯의 방향 표시를 확인한다.
대상: Run HUD(배달 카드·점수 표시 포함), 런 이벤트 전조, Home, Settings, Tutorial, Pause, RunResult, Shop, ReceiptShop, 상품 카드, FPS 표시, 카운트다운, 화면 전환 페이드.
실행: powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\\Tools\\Verify-Project.ps1 -SkipBuild -SkipBlueprints -SkipTests -WidgetQA
결과: Saved/Verify/WidgetQA/*.png, 로그의 '### WIDGET_QA: PASS' 또는 '### WIDGET_QA: FAIL'
"""
import os
from collections import Counter

import unreal

LIB = unreal.PGEditorAutomationLibrary
OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), 'Verify', 'WidgetQA')
WIDGETS = '/Game/Blueprints/Widgets/'
# 위젯 디자인 기준 해상도. DPI 배율 없이 1:1로 그린다.
SIZE = unreal.IntPoint(1080, 2340)
BACKGROUND = unreal.LinearColor(0.20, 0.36, 0.52, 1.0)
HIDDEN = unreal.SlateVisibility.COLLAPSED
# 버튼이 터치를 받는다고 인정할 최소 표본 수. 30px 격자 9칸은 약 90x90px 면적이다.
MIN_BUTTON_SAMPLES = 9
# 터치로 조작하는 위젯. 설정 화면의 슬라이더·체크박스도 버튼과 같은 기준으로 본다.
BUTTON_TYPES = (unreal.Button, unreal.CheckBox, unreal.Slider)
# 화면의 진입 애니메이션을 끝까지 돌리려고 흘려보내는 시간(초). 가장 긴 진입(RunResult 약 1.6초)보다 길면 된다.
SETTLE_SECONDS = 10.0
# 배달 카드 진입(0.16초)은 끝나고 점수 표시(0.85초, 0.38초부터 사라짐)는 아직 또렷한 시점(초).
CARGO_SECONDS = 0.2
failures = []
os.makedirs(OUT, exist_ok=True)


def log(message):
    unreal.log('### ' + str(message))


def check(condition, message):
    log(('ok   ' if condition else 'FAIL ') + message)
    if not condition:
        failures.append(message)


def create(class_path):
    widget_class = unreal.load_class(None, class_path)
    widget = LIB.create_widget_instance(widget_class) if widget_class else None
    check(widget is not None, f'create {class_path}')
    return widget


def create_widget(path):
    """Content/Blueprints/Widgets 아래 경로('Home/WBP_HomeScreen')로 인스턴스를 만든다."""
    return create(f'{WIDGETS}{path}.{path.rsplit("/", 1)[-1]}_C')


def enter(entries):
    # 그릴 때마다 Slate 위젯이 다시 만들어져 NativeConstruct가 화면을 진입 전(숨김·버튼 잠금)으로 되돌린다.
    # 그래서 그리기·히트 테스트 직전마다 게임이 부르는 진입 함수를 다시 부르고, advance로 Tick을 돌려 끝난 상태를 만든다.
    for entry in entries:
        LIB.play_widget_entry(entry)


def render(widget, name, entries=(), advance=0.0):
    path = os.path.join(OUT, name + '.png')
    # 에디터는 셰이더를 처음 그릴 때 요청해 비동기로 컴파일한다. 한 번 그려 요청을 만든 뒤 컴파일·스트리밍을 끝내고 다시 그린다.
    enter(entries)
    LIB.render_widget_to_png(widget, SIZE, BACKGROUND, path, advance)
    unreal.AutomationLibrary.finish_loading_before_screenshot()
    enter(entries)
    check(LIB.render_widget_to_png(widget, SIZE, BACKGROUND, path, advance), f'render {name}.png')


def children_of(widget):
    """위젯 인스턴스의 트리에 속한 UMG 위젯을 이름으로 찾을 수 있게 모은다."""
    return {w.get_name(): w for w in unreal.ObjectIterator(unreal.Widget)
            if w.get_outer() and w.get_outer().get_outer() == widget}


def hit_names(widget, step=30, entries=(), advance=0.0, ignore_enabled=False):
    """화면 전체를 격자로 찍어 표본마다 입력을 받는 위젯 이름을 구한다(받는 위젯이 없으면 'None')."""
    points = [unreal.Vector2D(float(x), float(y))
              for y in range(step // 2, SIZE.y, step) for x in range(step // 2, SIZE.x, step)]
    enter(entries)
    return [str(name) for name in LIB.find_hit_widget_names(widget, SIZE, points, advance, ignore_enabled)]


def hit_counts(widget, step=30, entries=(), advance=0.0, ignore_enabled=False):
    """화면 전체를 격자로 찍어 입력을 받는 위젯 이름별 표본 수를 구한다."""
    return Counter(hit_names(widget, step, entries, advance, ignore_enabled))


def descendants(widget):
    names = {widget.get_name()}
    if isinstance(widget, unreal.PanelWidget):
        for child in widget.get_all_children():
            names |= descendants(child)
    return names


def check_buttons_reachable(widget, label, entries=(), advance=0.0, may_be_locked=()):
    """버튼이 가려지지 않았고 터치를 받는지 확인하고, 버튼이 차지한 격자 표본 번호를 돌려준다.

    비활성 버튼 처리: 게임과 같은 히트 테스트는 비활성 위젯에서 경로가 끊겨, 잠긴 버튼은 가려지지 않았어도 표본이 0이다.
    그래서 두 가지를 따로 본다.
      1) 가림: 비활성 여부를 무시한 히트 테스트로 버튼 자리의 최상단 위젯이 버튼(또는 그 내용물)인지 본다. 모든 버튼에 적용한다.
      2) 터치: 진입 애니메이션이 끝난 상태에서 게임과 같은 히트 테스트로 실제 터치를 받는지 본다.
         may_be_locked에 적은 버튼(선택된 품질 프리셋처럼 게임 규칙상 꺼지는 버튼)이 꺼져 있으면 실패가 아니라 로그만 남긴다.
    """
    top = hit_names(widget, entries=entries, advance=advance, ignore_enabled=True)
    touched = hit_counts(widget, entries=entries, advance=advance)
    buttons = {name: descendants(child) for name, child in children_of(widget).items() if isinstance(child, BUTTON_TYPES)}
    # 버튼을 하나도 못 찾으면 아래 검사가 통째로 건너뛰어지므로 따로 실패로 잡는다.
    check(bool(buttons), f'{label}: found buttons to test')
    owned = set().union(*buttons.values())
    others = Counter(hit for hit in top if hit != 'None' and hit not in owned).most_common(5)
    occupied = set()
    for name, inside in sorted(buttons.items()):
        # 버튼 안의 이미지·텍스트가 입력을 받는 경우도 버튼에 도달한 것으로 본다.
        points = {index for index, hit in enumerate(top) if hit in inside}
        occupied |= points
        covered = len(points) < MIN_BUTTON_SAMPLES
        check(not covered, f'{label}: button {name} is not covered by another widget ({len(points)} samples, need {MIN_BUTTON_SAMPLES})'
              + (f'; other input takers: {others}' if covered else ''))
        samples = sum(touched[hit] for hit in inside)
        if name in may_be_locked and samples < MIN_BUTTON_SAMPLES:
            log(f'note {label}: button {name} is locked by game state in this capture (not a failure)')
        else:
            check(samples >= MIN_BUTTON_SAMPLES, f'{label}: button {name} receives touches ({samples} samples, need {MIN_BUTTON_SAMPLES})')
    return occupied


# ---------------------------------------------------------------- Run HUD
hud_buttons = set()
hud = create('/Game/Blueprints/Widgets/HUD/WBP_RunHUD.WBP_RunHUD_C')
if hud:
    render(hud, 'RunHUD')
    hud_buttons = check_buttons_reachable(hud, 'RunHUD')
    danger = hud.get_editor_property('CargoDangerImage')
    check(danger is not None and danger.get_visibility() == HIDDEN, 'RunHUD: CargoDangerImage is bound and hidden while inactive')
    hud.set_cargo_danger_warning_active(True)
    check(danger.get_visibility() == unreal.SlateVisibility.HIT_TEST_INVISIBLE, 'RunHUD: CargoDangerImage is shown without taking input while active')
    render(hud, 'RunHUD_CargoDanger')
    check_buttons_reachable(hud, 'RunHUD with danger warning')
    # 그릴 때마다 Slate 위젯이 다시 만들어진다. 재구성 뒤에도 경고가 켜진 상태로 남아야 위 그림과 검사가 유효하다.
    check(danger.get_visibility() == unreal.SlateVisibility.HIT_TEST_INVISIBLE, 'RunHUD: CargoDangerImage stays shown after the Slate widget is rebuilt')
    hud.set_cargo_danger_warning_active(False)
    check(danger.get_visibility() == HIDDEN, 'RunHUD: CargoDangerImage hides again')

    # 배달 카드와 점수 표시는 HUD 안에서만 제 크기로 그려진다. 소유 플레이어가 없어 HUD가 직접 만들지 못하므로 같은 자리에 넣어 준다.
    cargo_items = [create_widget('HUD/WBP_CargoDeliveryItem') for _ in range(3)]
    for item in cargo_items:
        hud.get_editor_property('CargoListBox').add_child(item)
    indicator = create_widget('HUD/WBP_CargoScoreIndicator')
    indicator_slot = hud.get_editor_property('CargoScoreIndicatorLayer').add_child_to_canvas(indicator)
    indicator_slot.set_auto_size(True)
    indicator_slot.set_alignment(unreal.Vector2D(0.5, 0.5))
    indicator_slot.set_position(unreal.Vector2D(300.0, 300.0))
    cargo_entries = cargo_items + [indicator]
    render(hud, 'RunHUD_Cargo', cargo_entries, CARGO_SECONDS)
    check_buttons_reachable(hud, 'RunHUD with cargo items', cargo_entries, CARGO_SECONDS)
    cargo_hits = hit_counts(hud, entries=cargo_entries, advance=CARGO_SECONDS, ignore_enabled=True)
    check(not any(cargo_hits[child.get_name()] for child in cargo_entries), 'RunHUD: cargo items and score indicator do not take input')

# ---------------------------------------------------------------- run event previews
WARNINGS = ('LeftWarning', 'CenterWarning', 'RightWarning')
for label, direction, expected in (('Left', -1.0, 'LeftWarning'), ('Center', 0.0, 'CenterWarning'), ('Right', 1.0, 'RightWarning')):
    preview = create('/Game/Blueprints/Widgets/RunEvents/WBP_MovingHazardPreview.WBP_MovingHazardPreview_C')
    if not preview:
        continue
    # 게임 코드와 같이 인스턴스 클래스에서 함수를 찾아 호출한다(BP 재정의가 있으면 그쪽이 실행된다).
    preview.call_method('ConfigureRunEventFeedback', args=(unreal.PGRunEventType.MOVING_HAZARD, unreal.PGRunEventState.PREVIEW, 3.0, 0.0, direction))
    widgets = children_of(preview)
    shown = [name for name in WARNINGS if widgets[name].get_visibility() != HIDDEN]
    check(shown == [expected], f'MovingHazardPreview direction {direction:+.0f} shows only {expected} (got {shown})')
    render(preview, 'MovingHazardPreview_' + label)
    check(set(hit_counts(preview, 90)) == {'None'}, f'MovingHazardPreview {label} does not take input')

# 기존 BP 그래프 구현(바람 전조)이 방향에 따라 좌우를 바꿔 표시하는지 확인한다.
wind = create('/Game/Blueprints/Widgets/RunEvents/WBP_WindPreview.WBP_WindPreview_C')
if wind:
    seen = []
    for direction in (-1.0, 1.0):
        wind.call_method('ConfigureRunEventFeedback', args=(unreal.PGRunEventType.WIND, unreal.PGRunEventState.PREVIEW, 3.0, 0.0, direction))
        widgets = children_of(wind)
        seen.append([name for name in ('LeftWarning', 'RightWarning') if widgets[name].get_render_opacity() > 0.5])
        render(wind, 'WindPreview_' + ('Negative' if direction < 0 else 'Positive'))
    check(sorted(seen) == [['LeftWarning'], ['RightWarning']], f'WindPreview blueprint graph switches sides with direction (got {seen})')

# ---------------------------------------------------------------- screens
# (PNG 이름, 에셋, 진입이 끝나도 게임 규칙상 꺼져 있을 수 있는 버튼)
SCREENS = (
    ('HomeScreen', 'Home/WBP_HomeScreen', ()),
    # 지금 선택된 품질 프리셋 버튼은 선택 표시로 꺼진다. 어느 것인지는 이 PC의 저장된 설정에 따른다.
    ('HomeSettingsScreen', 'Home/WBP_HomeSettingsScreen', ('LowPresetButton', 'BalancedPresetButton', 'HighPresetButton')),
    # 첫 페이지에서는 이전 버튼이 꺼진다.
    ('TutorialScreen', 'Home/WBP_TutorialScreen', ('PrevButton',)),
    ('RunPause', 'HUD/WBP_RunPause', ()),
    ('RunResult', 'RunResult/WBP_RunResult', ()),
    # 유저 데이터가 없어 탭 전환이 꺼진다. 카드 목록은 아래에서 직접 채운다.
    ('ShopScreen', 'Shop/WBP_ShopScreen', ('CharactersTabButton', 'MapsTabButton')),
    ('ReceiptShop', 'ReceiptShop/WBP_ReceiptShop', ()),
)
# 목록 화면에 넣을 카드. 게임은 소유 플레이어로 카드를 만들어 헤드리스에서는 목록이 비므로, 목록 영역이 가려지지 않았는지 보려고 직접 넣는다.
LIST_ITEMS = {
    'ShopScreen': ('ItemListBox', ('Shop/WBP_CharacterShopItem', 'Shop/WBP_CharacterShopItem', 'Shop/WBP_MapShopItem', 'Shop/WBP_MapShopItem')),
    'ReceiptShop': ('PackListBox', ('ReceiptShop/WBP_ReceiptShopPackItem',) * 4),
}
for name, path, may_be_locked in SCREENS:
    screen = create_widget(path)
    if not screen:
        continue
    if name == 'RunResult':
        # 자릿수가 많은 값으로 줄바꿈·잘림이 없는지 본다.
        screen.configure_run_result('Fell', 83.0, 1234.0, 125680, 12, 7)
    list_name, item_paths = LIST_ITEMS.get(name, (None, ()))
    items = [create_widget(item_path) for item_path in item_paths]
    for item in items:
        screen.get_editor_property(list_name).add_child(item)
    render(screen, name, [screen], SETTLE_SECONDS)
    check_buttons_reachable(screen, name, [screen], SETTLE_SECONDS, may_be_locked)
    if items:
        # 카드 내부는 중첩 위젯이라 카드 이름으로 집계된다. 카드 자체의 버튼은 아래 단독 검사에서 본다.
        item_hits = hit_counts(screen, entries=[screen], advance=SETTLE_SECONDS, ignore_enabled=True)
        for item in items:
            samples = item_hits[item.get_name()]
            check(samples >= MIN_BUTTON_SAMPLES, f'{name}: list item {item.get_name()} is not covered by another widget ({samples} samples, need {MIN_BUTTON_SAMPLES})')

# 카드는 단독으로 그리면 화면 크기로 늘어나 그림이 의미가 없다. PNG는 위 목록 화면으로 대신하고 여기서는 카드 안 버튼만 본다.
for path in ('Shop/WBP_CharacterShopItem', 'Shop/WBP_MapShopItem', 'ReceiptShop/WBP_ReceiptShopPackItem'):
    card = create_widget(path)
    if card:
        check_buttons_reachable(card, path.rsplit('/', 1)[-1])

# ---------------------------------------------------------------- overlays
# FPS 표시는 뷰포트 최상단(ZOrder 1000)에 떠 있어, 입력을 받으면 그 아래 모든 화면의 버튼을 가린다.
fps = create_widget('Etc/WBP_FPSDisplay')
if fps:
    render(fps, 'FPSDisplay')
    check(set(hit_counts(fps)) == {'None'}, f'FPSDisplay does not take input (got {hit_counts(fps).most_common(3)})')

# 카운트다운(ZOrder 20)은 Run HUD(ZOrder 15) 위에 뜬다. 숫자는 StartCountdown 뒤 월드 시간으로만 나타나는데
# 헤드리스 에디터 월드는 시간이 흐르지 않아 그릴 것이 없다. PNG는 건너뛰고 HUD 버튼을 가리지 않는지만 본다.
countdown = create_widget('Home/WBP_HomeCountdown')
if countdown:
    log('skip HomeCountdown.png: the digits only appear while the world clock runs after StartCountdown')
    blocked = {index for index, hit in enumerate(hit_names(countdown)) if hit != 'None'} & hud_buttons
    check(not blocked, f'HomeCountdown does not take input over Run HUD buttons ({len(blocked)} samples overlap)')

# 화면 전환 페이드(ZOrder 500)는 덮는 동안 화면 전체의 입력을 막고 끝나면 스스로 제거된다. 의도된 동작이라 그림만 남긴다.
fade = create_widget('Etc/WBP_ScreenTransitionFade')
if fade:
    render(fade, 'ScreenTransitionFade', [fade], 1.0)
    log(f'note ScreenTransitionFade input takers while covering: {hit_counts(fade, entries=[fade], advance=1.0).most_common(3)}')

log('WIDGET_QA: ' + ('PASS' if not failures else f'FAIL ({len(failures)}): ' + ' | '.join(failures)))
