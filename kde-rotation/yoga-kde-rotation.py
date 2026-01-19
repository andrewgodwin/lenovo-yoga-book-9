#!/usr/bin/env python3

import subprocess
import json
import time


# The display that is already rotating automatically
PRIMARY = "eDP-1"

# The display you want to sync
SECONDARY = "eDP-2"

# Screen dimensions
WIDTH = 2880
HEIGHT = 1800

rotation_name_map = {
    1: "normal",
    2: "left",
    4: "inverted",
    8: "right",
}

while True:
    # Get display 1's rotation
    doctor_output = json.loads(subprocess.check_output(["kscreen-doctor", "-j"]))
    output_1_rotation = None
    output_2_rotation = None
    output_1_scale = None
    output_2_scale = None
    for output in doctor_output["outputs"]:
        if output["name"] == PRIMARY:
            output_1_rotation = rotation_name_map[output["rotation"]]
            output_1_scale = output["scale"]
        elif output["name"] == SECONDARY:
            output_2_rotation = rotation_name_map[output["rotation"]]
            output_2_scale = output["scale"]

    if output_1_rotation is None:
        print(f"Warning: Could not find rotation for display {PRIMARY}")
        time.sleep(10)
        continue
    if output_1_scale is None or output_2_scale is None:
        print(f"Warning: Could not find display scales")
        time.sleep(10)
        continue

    print("currently", output_1_rotation, output_2_rotation)

    # Apply it to display 2. Note that display 1 is physically upside down.
    if output_1_rotation == "normal":
        # Machine is upside down
        output_1_position = f"0,{int(HEIGHT/output_2_scale)}"
        output_2_position = "0,0"
        output_2_new_rotation = "inverted"
    elif output_1_rotation == "left":
        # Machine is on its right edge
        output_1_position = f"{int(HEIGHT/output_1_scale)},0"
        output_2_position = "0,0"
        output_2_new_rotation = "right"
    elif output_1_rotation == "right":
        # Machine is on its left edge
        output_1_position = f"{int(HEIGHT/output_1_scale)},0"
        output_2_position = "0,0"
        output_2_new_rotation = "left"
    else:
        # Machine is upright.
        output_1_position = "0,0"
        output_2_position = f"0,{int(HEIGHT/output_1_scale)}"
        output_2_new_rotation = "normal"
    # rotation_name = {
    #     1: "inverted",
    #     2: "right",
    #     4: "normal",
    #     8: "left",
    # }[display_rotation]
    if output_2_rotation != output_2_new_rotation:
        print(repr(output_2_rotation), "old / new", repr(output_2_new_rotation))
        subprocess.check_call(
            [
                "kscreen-doctor",
                f"output.{SECONDARY}.rotation.{output_2_rotation}",
            ]
        )
        subprocess.check_call(
            [
                "kscreen-doctor",
                f"output.{PRIMARY}.position.{output_1_position}",
                f"output.{SECONDARY}.position.{output_2_position}",
            ]
        )
    time.sleep(1)
