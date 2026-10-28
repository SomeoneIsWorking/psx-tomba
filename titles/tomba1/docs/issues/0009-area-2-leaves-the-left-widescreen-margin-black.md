# 0009: area 2 leaves the left widescreen margin black

Status: open (S005).

## Reproduction

`tomba2_port scratch/bin/tomba1/SCUS_942.36`, hidden, unpaced, ires 0, sink 1920x1080, aspect 16:9: Start at the
title until the first field, then `warp 1`, `warp 2`, a `shot` 25 s after each.

## Observed vs expected

Area 1 fills both margins. Area 2 (the village) draws nothing in the left 54-column margin; the right margin is
filled. Expected: both margins show the scene.

## Cause

Not traced. S005 records that the guest keeps its retail culls at OFX 160, so a primitive left of x 0 is
culled before it reaches the record.

## Proper fix

S005: the Tomba! 1 screen-edge culls and projection contributions take the wide draw window, then a controlled
4:3 run. The README carries no Tomba! 1 media until it lands.
