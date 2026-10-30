"""위젯 시각·입력 회귀 검증 (헤드리스 에디터 Python).

위젯을 실제 RHI로 그려 PNG를 남기고, 버튼이 장식 위젯에 가려지지 않았는지와 런 이벤트 배지의 방향 표시를 확인한다.
대상: Run HUD(배달 카드·점수 표시·손실 임박 카드 경고 포함), 첫 런 가이드(드래그·플릭 단계), 런 이벤트 배지(전조·발동), Home, Settings, Tutorial, Pause, RunResult, Shop, ReceiptShop, 상품 카드, FPS 표시, 카운트다운, 화면 전환 페이드.
실행: powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\\Tools\\Verify-Project.ps1 -SkipBuild -SkipBlueprints -SkipTests -WidgetQA
결과: Temp/Verify/WidgetQA/*.png(Git 제외, 언제든 비워도 됨), 로그의 '### WIDGET_QA: PASS' 또는 '### WIDGET_QA: FAIL'
언어: 스크립트 인자로 문화권(en)을 주면 그 언어의 locres(Content/Localization/Game)를 미리보기로 적용해 Temp/Verify/WidgetQA/<문화권>/에 그린다.
      인자가 없으면 원어(한국어 원문)로 그린다. Verify-Project.ps1은 두 번(원어, en) 실행한다.
      그린 화면마다 문구가 배정된 칸보다 커서 잘리거나 겹치는 TextBlock이 없는지(find_overflowing_text_blocks) 함께 검사한다.
화면 비율·절개 영역: 디자인 해상도(1080×2340) 뒤에 16:9, 태블릿 16:10, 21:9와 펀치홀 위 여백(96px, SafeZone 흉내)으로 HUD·화면을 다시 그려
      Temp/Verify/WidgetQA/<문화권>/<패스>/에 남기고 버튼 도달·문구 넘침을 같은 기준으로 검사한다. 로딩 화면은 LoadingScreen.png로 그린다.
"""
import os
import sys
from collections import Counter

import unreal

LIB = unreal.PGEditorAutomationLibrary
OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), 'Temp', 'Verify', 'WidgetQA')
CULTURE = sys.argv[1] if len(sys.argv) > 1 else ''
if CULTURE:
    # 에디터 프로세스는 게임 locres를 자동으로 읽지 않으므로 미리보기로 켠다(에디터 언어는 그대로).
    LIB.set_game_localization_preview(CULTURE)
    OUT = os.path.join(OUT, CULTURE)
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
    # 현지화로 길어진 문구가 칸을 넘치면 잘리거나 이웃과 겹친다. 그린 상태 그대로 TextBlock의 원하는 크기와 배정 크기를 비교한다.
    enter(entries)
    overflow = [str(item) for item in LIB.find_overflowing_text_blocks(widget, SIZE, advance)]
    check(not overflow, f'{name}: every text fits its box' + (f' {overflow}' if overflow else ''))


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

    # 손실 임박 카드 경고: 문턱(Cargo > CargoDangerWarningRatio)을 넘은 적재물의 카드만 테두리를 켠다.
    frames = [children_of(item)['DangerFrame'] for item in cargo_items]
    check(all(frame.get_visibility() == HIDDEN for frame in frames), 'CargoDeliveryItem: DangerFrame is hidden while safe')
    cargo_items[0].set_danger_ratio(0.9)
    cargo_items[1].set_danger_ratio(0.3)
    check(frames[0].get_visibility() == unreal.SlateVisibility.HIT_TEST_INVISIBLE and frames[1].get_visibility() == HIDDEN,
          'CargoDeliveryItem: only the endangered card shows DangerFrame without taking input')
    render(hud, 'RunHUD_CargoDanger', cargo_entries, CARGO_SECONDS)
    check(frames[0].get_visibility() == unreal.SlateVisibility.HIT_TEST_INVISIBLE, 'CargoDeliveryItem: DangerFrame stays shown after the Slate widget is rebuilt')
    cargo_items[0].set_danger_ratio(0.0)
    check(frames[0].get_visibility() == HIDDEN, 'CargoDeliveryItem: DangerFrame hides again')

# ---------------------------------------------------------------- run event badges
# (WBP, 이벤트, 방향 -> 보여야 할 방향 표시 이름. None이면 방향 표시가 없다)
DIRECTIONS = ('DirectionNegative', 'DirectionZero', 'DirectionPositive')
BADGES = (
    ('RunEvents/WBP_RunEventBadge_Wind', unreal.PGRunEventType.WIND, ((-1.0, 'DirectionNegative'), (1.0, 'DirectionPositive'))),
    ('RunEvents/WBP_RunEventBadge_Snowstorm', unreal.PGRunEventType.SNOWSTORM, ((0.0, None),)),
    ('RunEvents/WBP_RunEventBadge_MovingHazard', unreal.PGRunEventType.MOVING_HAZARD, ((-1.0, 'DirectionNegative'), (0.0, 'DirectionZero'), (1.0, 'DirectionPositive'))),
)
for path, event_type, cases in BADGES:
    label = path.rsplit('_', 1)[-1]
    for direction, expected in cases:
        badge = create_widget(path)
        if not badge:
            continue
        # 게임 코드와 같이 인스턴스 클래스에서 함수를 찾아 호출한다. 전조: 3초 뒤 발동, 링이 1.5초만큼 차오른 장면을 그린다.
        badge.call_method('ConfigureRunEventFeedback', args=(event_type, unreal.PGRunEventState.PREVIEW, 3.0, 0.0, direction))
        widgets = children_of(badge)
        shown = [name for name in DIRECTIONS if name in widgets and widgets[name].get_visibility() != HIDDEN]
        check(shown == ([expected] if expected else []), f'{label} badge direction {direction:+.0f} shows {expected} (got {shown})')
        check(widgets['NameText'].get_visibility() != HIDDEN, f'{label} badge shows the event name during preview')
        suffix = '' if len(cases) == 1 else '_' + ('Negative' if direction < 0 else 'Zero' if direction == 0 else 'Positive')
        render(badge, f'RunEventBadge_{label}_Preview{suffix}', advance=1.5)
        check(set(hit_counts(badge, 90, advance=1.5)) == {'None'}, f'{label} badge preview does not take input')
    # 발동: 이름을 숨기고 칩으로 줄어든 뒤 링이 남은 시간(4초 중 2.5초)만큼 남는다.
    badge = create_widget(path)
    if badge:
        badge.call_method('ConfigureRunEventFeedback', args=(event_type, unreal.PGRunEventState.TRIGGERED, 0.0, 4.0, cases[0][0]))
        widgets = children_of(badge)
        check(widgets['NameText'].get_visibility() == HIDDEN, f'{label} badge hides the event name once triggered')
        render(badge, f'RunEventBadge_{label}_Active', advance=1.5)
        scale = widgets['BadgePanel'].get_editor_property('render_transform').scale
        check(abs(scale.x - 0.62) < 0.01, f'{label} badge shrinks to the chip scale after trigger (got {scale.x:.2f})')

# ---------------------------------------------------------------- screens
# (PNG 이름, 에셋, 진입이 끝나도 게임 규칙상 꺼져 있을 수 있는 버튼)
SCREENS = (
    ('HomeScreen', 'Home/WBP_HomeScreen', ()),
    # 지금 선택된 품질 프리셋 버튼은 선택 표시로 꺼진다. 어느 것인지는 이 PC의 저장된 설정에 따른다. 현재 언어의 버튼도 같은 방식으로 꺼진다(에디터는 영어).
    ('HomeSettingsScreen', 'Home/WBP_HomeSettingsScreen', ('LowPresetButton', 'BalancedPresetButton', 'HighPresetButton', 'KoreanLanguageButton', 'EnglishLanguageButton')),
    # 첫 페이지에서는 이전 버튼이 꺼진다.
    ('TutorialScreen', 'Home/WBP_TutorialScreen', ('PrevButton',)),
    ('RunPause', 'HUD/WBP_RunPause', ()),
    ('QuitConfirm', 'Home/WBP_QuitConfirm', ()),
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
def qa_screens(label_suffix=''):
    """목록 화면 8종을 실제 데이터(긴 이름·큰 점수·카드)로 그리고 버튼 도달과 문구 넘침을 검사한다. 비율·절개 패스에서도 같은 기준으로 다시 돈다."""
    for name, path, may_be_locked in SCREENS:
        screen = create_widget(path)
        if not screen:
            continue
        label = name + label_suffix
        if name == 'RunResult':
            # 자릿수가 많은 값으로 줄바꿈·잘림이 없는지 본다. 이어하기는 제안·구매 가능 상태(버튼 셋 다 켜짐)로 그린다.
            screen.configure_run_result('Fall', 83.0, 1234.0, 125680, 12, 7)
            screen.configure_continue_option(True, True, 20)
        if name == 'ShopScreen':
            # 선택 이름은 런타임 데이터(캐릭터·맵 이름)라 가장 긴 이름으로 명판 안에 들어가는지 본다. 긴 이름은 ScaleBox(DownOnly)가 줄인다.
            children_of(screen)['SelectedItemNameText'].set_text(unreal.Text('Sunshine Coast\u00a0Road' if CULTURE == 'en' else '브리즈 시티로드'))
        list_name, item_paths = LIST_ITEMS.get(name, (None, ()))
        items = [create_widget(item_path) for item_path in item_paths]
        for item in items:
            screen.get_editor_property(list_name).add_child(item)
        render(screen, name, [screen], SETTLE_SECONDS)
        check_buttons_reachable(screen, label, [screen], SETTLE_SECONDS, may_be_locked)
        if items:
            # 카드 내부는 중첩 위젯이라 카드 이름으로 집계된다. 카드 자체의 버튼은 아래 단독 검사에서 본다.
            item_hits = hit_counts(screen, entries=[screen], advance=SETTLE_SECONDS, ignore_enabled=True)
            for item in items:
                samples = item_hits[item.get_name()]
                check(samples >= MIN_BUTTON_SAMPLES, f'{label}: list item {item.get_name()} is not covered by another widget ({samples} samples, need {MIN_BUTTON_SAMPLES})')


qa_screens()

# 결과 화면의 이어하기 잠김(잔액 부족: 버튼이 흐려지고 입력을 안 받음)과 접힘(제안 없음: 다시 도전·홈만 남음) 상태.
for label, offered, affordable in (('RunResult_NoReceipts', True, False), ('RunResult_NoContinue', False, False)):
    screen = create_widget('RunResult/WBP_RunResult')
    if screen:
        screen.configure_run_result('CargoLost', 83.0, 1234.0, 125680, 12, 7)
        screen.configure_continue_option(offered, affordable, 20)
        render(screen, label, [screen], SETTLE_SECONDS)
        widgets = children_of(screen)
        hits = hit_counts(screen, entries=[screen], advance=SETTLE_SECONDS)
        continue_hits = sum(hits[name] for name in descendants(widgets['ContinueButton']))
        check(continue_hits == 0, f'{label}: ContinueButton does not take touches ({continue_hits} samples)')
        if offered:
            check_buttons_reachable(screen, label, [screen], SETTLE_SECONDS, ('ContinueButton',))
        else:
            # 접힌 버튼은 자리 자체가 없어 가림 검사에서 제외하고, 남은 두 버튼만 터치를 받는지 본다.
            check(widgets['ContinueButtonPanel'].get_visibility() == HIDDEN, f'{label}: ContinueButtonPanel is collapsed')
            for name in ('RetryButton', 'HomeButton'):
                samples = sum(hits[hit] for hit in descendants(widgets[name]))
                check(samples >= MIN_BUTTON_SAMPLES, f'{label}: button {name} receives touches ({samples} samples, need {MIN_BUTTON_SAMPLES})')

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

# 첫 런 가이드(ZOrder 16)는 Run HUD 위에 겹치므로 어느 단계에서도 입력을 받지 않아야 한다. 페이드 인(0.25초)이 끝난 뒤 그린다.
guide = create_widget('HUD/WBP_RunGuide')
if guide:
    for step, label in ((unreal.PGRunGuideStep.DRAG, 'Drag'), (unreal.PGRunGuideStep.FLICK, 'Flick')):
        guide.show_step(step)
        render(guide, 'RunGuide_' + label, advance=0.6)
        check(set(hit_counts(guide, 90, advance=0.6)) == {'None'}, f'RunGuide {label} does not take input')

# 화면 전환 페이드(ZOrder 500)는 덮는 동안 화면 전체의 입력을 막고 끝나면 스스로 제거된다. 의도된 동작이라 그림만 남긴다.
fade = create_widget('Etc/WBP_ScreenTransitionFade')
if fade:
    render(fade, 'ScreenTransitionFade', [fade], 1.0)
    log(f'note ScreenTransitionFade input takers while covering: {hit_counts(fade, entries=[fade], advance=1.0).most_common(3)}')

# ---------------------------------------------------------------- 로딩 화면
# 레벨 전환 중 MoviePlayer가 그리는 로딩 화면(UPGLoadingScreenSubsystem)은 PIE·헤드리스에서 볼 수 없어 같은 Slate 위젯을 캡처한다.
loading_path = os.path.join(OUT, 'LoadingScreen.png')
LIB.render_loading_screen_to_png(SIZE, loading_path)
unreal.AutomationLibrary.finish_loading_before_screenshot()
check(LIB.render_loading_screen_to_png(SIZE, loading_path), 'render LoadingScreen.png')

# ---------------------------------------------------------------- 다른 화면 비율·절개 영역
# 디자인 해상도 밖의 비율에서 HUD와 화면이 잘리거나 겹치지 않는지, 절개 영역(SafeZone) 여백이 가장자리 UI를 안쪽으로 옮기는지 본다.
# 크기는 DPI 규칙(짧은 변 1080 = 배율 1) 기준 디자인 픽셀이다: 태블릿 1600×2560은 배율 1.48이라 1080×1728로 그린다.
LAYOUT_PASSES = (
    ('16x9', unreal.IntPoint(1080, 1920), 0.0),
    ('tablet16x10', unreal.IntPoint(1080, 1728), 0.0),
    ('21x9', unreal.IntPoint(1080, 2520), 0.0),
    # 펀치홀·노치 기기의 위쪽 절개 여백. S24+·Z Flip4 상태 표시줄 높이를 1080px 폭 기준으로 환산한 값이다.
    ('cutout96', unreal.IntPoint(1080, 2340), 96.0),
)
# 절개 패스에서 SafeZone 안의 가장자리 UI(점수·Pause·영수증·도움말·닫기)가 여백 아래로 내려와야 하는 위젯.
SAFE_ZONE_WIDGETS = ('HUD/WBP_RunHUD', 'Home/WBP_HomeScreen', 'Shop/WBP_ShopScreen', 'Home/WBP_TutorialScreen', 'Etc/WBP_FPSDisplay')
BASE_OUT, BASE_SIZE = OUT, SIZE
for pass_label, pass_size, safe_top in LAYOUT_PASSES:
    OUT, SIZE = os.path.join(BASE_OUT, pass_label), pass_size
    os.makedirs(OUT, exist_ok=True)
    LIB.set_safe_zone_emulation(SIZE, unreal.Margin(0.0, safe_top, 0.0, 0.0))
    suffix = f'@{pass_label}'
    hud = create('/Game/Blueprints/Widgets/HUD/WBP_RunHUD.WBP_RunHUD_C')
    if hud:
        render(hud, 'RunHUD')
        check_buttons_reachable(hud, 'RunHUD' + suffix)
    qa_screens(suffix)
    if safe_top > 0.0:
        # 여백 위 격자 줄(y < safe_top)에 버튼(과 그 내용물)이 있으면 안 된다. 전체 화면 장식 그림이 입력을 막는 것은 상관없다.
        rows_above = int(safe_top) // 30 + (1 if int(safe_top) % 30 > 15 else 0)
        columns = len(range(15, SIZE.x, 30))
        for path in SAFE_ZONE_WIDGETS:
            widget = create_widget(path)
            if not widget:
                continue
            buttons = set().union(*(descendants(child) for child in children_of(widget).values() if isinstance(child, BUTTON_TYPES)))
            hits = hit_names(widget, entries=[widget], advance=SETTLE_SECONDS, ignore_enabled=True)
            intruders = sorted({hit for hit in hits[:rows_above * columns] if hit in buttons})
            check(not intruders, f'{path.rsplit("/", 1)[-1]}{suffix}: no button above the cutout inset (got {intruders})')
    LIB.set_safe_zone_emulation(SIZE, unreal.Margin(0.0, 0.0, 0.0, 0.0))
OUT, SIZE = BASE_OUT, BASE_SIZE

log(f'WIDGET_QA culture={CULTURE or "native"}: ' + ('PASS' if not failures else f'FAIL ({len(failures)}): ' + ' | '.join(failures)))
log('WIDGET_QA: ' + ('PASS' if not failures else f'FAIL ({len(failures)})'))
