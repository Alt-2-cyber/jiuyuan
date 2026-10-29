"""Detect and track people in a video with Ultralytics YOLO.

The script writes an MP4 video at the source resolution and frame rate. It only
keeps COCO class 0 (person) and draws its own stable, high-contrast labels.
"""

from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path

import cv2
import torch
from ultralytics import YOLO


PERSON_CLASS = 0
BOX_COLOR = (0, 224, 104)
TEXT_COLOR = (18, 22, 20)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="Source video path")
    parser.add_argument("output", type=Path, help="Annotated MP4 output path")
    parser.add_argument("--model", default="yolo11s.pt", help="Ultralytics YOLO detection model")
    parser.add_argument("--imgsz", type=int, default=1280, help="Inference image size")
    parser.add_argument("--conf", type=float, default=0.20, help="Person confidence threshold")
    parser.add_argument("--max-frames", type=int, default=0, help="Process at most this many frames; 0 means all")
    parser.add_argument("--start-frame", type=int, default=0, help="Frame index to start from")
    parser.add_argument("--no-track", action="store_true", help="Disable ByteTrack IDs")
    return parser.parse_args()


def video_info(source: Path) -> dict[str, float | int]:
    capture = cv2.VideoCapture(str(source))
    if not capture.isOpened():
        raise RuntimeError(f"Cannot open source video: {source}")
    info = {
        "width": int(capture.get(cv2.CAP_PROP_FRAME_WIDTH)),
        "height": int(capture.get(cv2.CAP_PROP_FRAME_HEIGHT)),
        "fps": capture.get(cv2.CAP_PROP_FPS),
        "frames": int(capture.get(cv2.CAP_PROP_FRAME_COUNT)),
    }
    capture.release()
    return info


def choose_device() -> str:
    return "0" if torch.cuda.is_available() else "cpu"


def draw_label(frame, x1: int, y1: int, x2: int, y2: int, label: str) -> None:
    height, width = frame.shape[:2]
    x1 = max(0, min(x1, width - 1))
    y1 = max(0, min(y1, height - 1))
    x2 = max(0, min(x2, width - 1))
    y2 = max(0, min(y2, height - 1))
    thickness = max(2, round(min(width, height) / 720))
    font_scale = max(0.55, min(width, height) / 1500)
    cv2.rectangle(frame, (x1, y1), (x2, y2), BOX_COLOR, thickness, cv2.LINE_AA)
    (label_width, label_height), baseline = cv2.getTextSize(label, cv2.FONT_HERSHEY_SIMPLEX, font_scale, thickness)
    label_y1 = max(0, y1 - label_height - baseline - 8)
    label_y2 = min(height, y1)
    cv2.rectangle(frame, (x1, label_y1), (min(width - 1, x1 + label_width + 10), label_y2), BOX_COLOR, -1)
    cv2.putText(
        frame,
        label,
        (x1 + 5, label_y2 - baseline - 4),
        cv2.FONT_HERSHEY_SIMPLEX,
        font_scale,
        TEXT_COLOR,
        thickness,
        cv2.LINE_AA,
    )


def annotate_frame(frame, result) -> int:
    boxes = result.boxes
    count = 0
    if boxes is None or len(boxes) == 0:
        return count

    coordinates = boxes.xyxy.cpu().tolist()
    confidences = boxes.conf.cpu().tolist()
    track_ids = boxes.id.int().cpu().tolist() if boxes.id is not None else [None] * len(coordinates)
    for box, confidence, track_id in zip(coordinates, confidences, track_ids):
        x1, y1, x2, y2 = (round(value) for value in box)
        identifier = f" #{track_id}" if track_id is not None else ""
        draw_label(frame, x1, y1, x2, y2, f"PERSON{identifier} {confidence:.0%}")
        count += 1
    return count


def main() -> int:
    args = parse_args()
    if not args.input.is_file():
        raise FileNotFoundError(f"Input video does not exist: {args.input}")
    if args.conf <= 0 or args.conf >= 1:
        raise ValueError("--conf must be between 0 and 1")
    if args.imgsz < 320:
        raise ValueError("--imgsz must be at least 320")

    info = video_info(args.input)
    if info["fps"] <= 0 or info["width"] <= 0 or info["height"] <= 0:
        raise RuntimeError(f"Invalid source video metadata: {json.dumps(info)}")
    device = choose_device()
    print(json.dumps({"source": str(args.input), **info, "device": device}, ensure_ascii=False), flush=True)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    capture = cv2.VideoCapture(str(args.input))
    capture.set(cv2.CAP_PROP_POS_FRAMES, args.start_frame)
    fourcc = cv2.VideoWriter_fourcc(*"mp4v")
    writer = cv2.VideoWriter(str(args.output), fourcc, info["fps"], (info["width"], info["height"]))
    if not writer.isOpened():
        raise RuntimeError(f"Cannot open output video writer: {args.output}")

    model = YOLO(args.model)
    start_time = time.monotonic()
    frame_index = args.start_frame
    processed = 0
    detected_frames = 0
    total_people = 0

    try:
        while True:
            ok, frame = capture.read()
            if not ok:
                break
            if args.no_track:
                result = model.predict(
                    frame,
                    classes=[PERSON_CLASS],
                    conf=args.conf,
                    imgsz=args.imgsz,
                    device=device,
                    verbose=False,
                )[0]
            else:
                result = model.track(
                    frame,
                    persist=True,
                    classes=[PERSON_CLASS],
                    conf=args.conf,
                    imgsz=args.imgsz,
                    device=device,
                    tracker="bytetrack.yaml",
                    verbose=False,
                )[0]
            people = annotate_frame(frame, result)
            if people:
                detected_frames += 1
                total_people += people
            writer.write(frame)
            processed += 1
            frame_index += 1

            if processed % 120 == 0:
                elapsed = max(time.monotonic() - start_time, 0.001)
                fps = processed / elapsed
                print(
                    f"PROGRESS frames={processed} source_frame={frame_index} "
                    f"rate={fps:.2f}fps detected_frames={detected_frames} people={total_people}",
                    flush=True,
                )
            if args.max_frames and processed >= args.max_frames:
                break
    finally:
        capture.release()
        writer.release()

    elapsed = time.monotonic() - start_time
    summary = {
        "processed_frames": processed,
        "source_start_frame": args.start_frame,
        "output": str(args.output),
        "detected_frames": detected_frames,
        "total_person_detections": total_people,
        "elapsed_seconds": round(elapsed, 2),
        "average_fps": round(processed / max(elapsed, 0.001), 2),
    }
    print("SUMMARY=" + json.dumps(summary, ensure_ascii=False), flush=True)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        print(f"ERROR: {error}", file=sys.stderr, flush=True)
        raise
