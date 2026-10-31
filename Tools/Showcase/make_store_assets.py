# -*- coding: utf-8 -*-
"""Play 스토어 그래픽(스크린샷·피처 그래픽·아이콘)을 쇼케이스 프레임에서 만든다.

입력: Tools/Showcase/shot_run.txt·shot_home_shop.txt를 -KeepFrames로 녹화한 프레임 폴더(1080×1920, 10fps).
출력: Docs/Release/StoreAssets/ — screenshots/NN_<장면>.jpg(1080×1920), feature_graphic.png(1024×500), icon_512.png.

사용: python Tools/Showcase/make_store_assets.py [--run Temp/Showcase/shot_run/frames] [--home Temp/Showcase/shot_home_shop/frames]
장면과 프레임 번호(10fps, 프레임 = 초×10)는 아래 SCENES에서 고친다. 장면을 다시 뽑을 때는 녹화 → 콘택트 시트로 확인 → 번호 수정 → 실행.
"""
import argparse
import os
import sys

from PIL import Image

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
OUT = os.path.join(ROOT, 'Docs', 'Release', 'StoreAssets')
LOGO = os.path.join(ROOT, 'Resources', 'Branding', 'Logo.png')
ICON = os.path.join(ROOT, 'Resources', 'Branding', 'AndroidAppIcon.png')

# (출력 이름, 프레임 출처 'run'|'home', 프레임 번호). 순서가 스토어 노출 순서다.
SCENES = [
    ('01_cargo_stacked', 'run', 254),      # 적재물 3개(배달 카드 3장) 얹고 다리 위
    ('02_delivery_complete', 'run', 262),  # 배달 완료 +1,150
    ('03_wind', 'run', 98),                # 바람 배지, 러너가 기운 상태
    ('04_snowstorm', 'run', 215),          # 눈보라 입자, 돌다리
    ('05_crow_warning', 'run', 28),        # 까마귀 전조 배지
    ('06_result', 'run', 340),             # 결과 화면(배달 실패, 이어하기 버튼)
    ('07_character_shop', 'home', 65),
    ('08_map_shop', 'home', 100),
    ('09_home', 'home', 30),               # 예비(Play는 폰 스크린샷 최대 8장)
]
# 피처 그래픽: (출처, 프레임, 크롭 왼쪽 x, 크롭 위쪽 y). 1000×488 크롭(1024:500 비율)을 1024×500으로 맞추고 로고를 오른쪽에 올린다.
# 러너가 화면 중앙보다 왼쪽에 오도록 왼쪽을 80px 잘라 오른쪽 손과 로고가 겹치지 않게 한다.
FEATURE = ('run', 254, 80, 780)


def frame_path(frames_dir, index):
    path = os.path.join(frames_dir, 'frame_%05d.png' % index)
    if not os.path.isfile(path):
        sys.exit('missing frame: ' + path)
    return path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--run', default=os.path.join(ROOT, 'Temp', 'Showcase', 'shot_run', 'frames'))
    parser.add_argument('--home', default=os.path.join(ROOT, 'Temp', 'Showcase', 'shot_home_shop', 'frames'))
    args = parser.parse_args()
    sources = {'run': args.run, 'home': args.home}

    shots_dir = os.path.join(OUT, 'screenshots')
    os.makedirs(shots_dir, exist_ok=True)
    for name, source, index in SCENES:
        image = Image.open(frame_path(sources[source], index)).convert('RGB')
        if image.size != (1080, 1920):
            sys.exit('unexpected frame size %r for %s' % (image.size, name))
        out = os.path.join(shots_dir, name + '.jpg')
        image.save(out, 'JPEG', quality=92, optimize=True)
        print('wrote', os.path.relpath(out, ROOT), os.path.getsize(out) // 1024, 'KB')

    # 피처 그래픽 1024×500: 알파 없는 PNG. 로고 외 텍스트는 넣지 않는다(StoreListing.ko.md).
    source, index, left, top = FEATURE
    frame = Image.open(frame_path(sources[source], index)).convert('RGB')
    feature = frame.crop((left, top, left + 1000, top + 488)).resize((1024, 500), Image.LANCZOS)
    logo = Image.open(LOGO).convert('RGBA')
    logo_width = 340
    logo = logo.resize((logo_width, int(logo.height * logo_width / logo.width)), Image.LANCZOS)
    feature.paste(logo, (1024 - logo_width - 24, (500 - logo.height) // 2), logo)
    feature_out = os.path.join(OUT, 'feature_graphic.png')
    feature.save(feature_out, 'PNG', optimize=True)
    print('wrote', os.path.relpath(feature_out, ROOT), feature.size)

    icon = Image.open(ICON).convert('RGBA')
    if icon.size != (512, 512):
        icon = icon.resize((512, 512), Image.LANCZOS)
    icon_out = os.path.join(OUT, 'icon_512.png')
    icon.save(icon_out, 'PNG', optimize=True)
    print('wrote', os.path.relpath(icon_out, ROOT), icon.size)


if __name__ == '__main__':
    main()
