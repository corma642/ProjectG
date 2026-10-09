"""Encodes a numbered image sequence (plus optional WAV cues) into an H.264 MP4 with Blender's bundled FFmpeg.

blender.exe -b --factory-startup --python encode_video.py -- <frames_dir> <out.mp4> <fps> <width> <height> [wav_path@seconds ...]
Frames are every .png/.jpg in frames_dir, sorted by name. Width and height must be even.
"""
import os
import sys

import bpy

argv = sys.argv[sys.argv.index('--') + 1:]
frames_dir, out_path, fps, width, height = argv[0], argv[1], int(argv[2]), int(argv[3]), int(argv[4])
cues = [a.rsplit('@', 1) for a in argv[5:]]

files = sorted(f for f in os.listdir(frames_dir) if f.lower().endswith(('.png', '.jpg', '.jpeg')))
if not files:
    raise SystemExit('no frames in ' + frames_dir)

scene = bpy.context.scene
scene.render.resolution_x, scene.render.resolution_y = width, height
scene.render.resolution_percentage = 100
scene.render.fps = fps
scene.frame_start, scene.frame_end = 1, len(files)
# 시퀀서 출력이 톤매핑으로 바뀌지 않게 표준 변환을 쓴다.
scene.view_settings.view_transform = 'Standard'

editor = scene.sequence_editor_create()
strips = editor.strips if hasattr(editor, 'strips') else editor.sequences
image = strips.new_image(name='frames', filepath=os.path.join(frames_dir, files[0]), channel=1, frame_start=1, fit_method='FIT')
for name in files[1:]:
    image.elements.append(name)
for index, (wav, seconds) in enumerate(cues):
    strips.new_sound(name=f'cue{index}', filepath=wav, channel=2 + index, frame_start=1 + round(float(seconds) * fps))

# Blender 5부터는 영상 형식을 고르기 전에 출력 종류를 영상으로 바꿔야 한다.
if hasattr(scene.render.image_settings, 'media_type'):
    scene.render.image_settings.media_type = 'VIDEO'
scene.render.image_settings.file_format = 'FFMPEG'
ffmpeg = scene.render.ffmpeg
ffmpeg.format = 'MPEG4'
ffmpeg.codec = 'H264'
ffmpeg.constant_rate_factor = 'MEDIUM'
ffmpeg.ffmpeg_preset = 'GOOD'
ffmpeg.gopsize = fps
ffmpeg.audio_codec = 'AAC' if cues else 'NONE'
scene.render.use_file_extension = False
scene.render.filepath = out_path
bpy.ops.render.render(animation=True)
print(f'### encoded {len(files)} frames -> {out_path} exists={os.path.exists(out_path)}')
