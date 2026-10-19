# TreeV: layout and camera traps

TreeV lays the tree out in cylindrical coordinates: each folder is a curved platform on a ring,
and its subfolders sit one ring further out. The code is
[src/geometry/TreeVLayout.cpp](../src/geometry/TreeVLayout.cpp) (ported from fsv's `geometry.c`)
and the TreeV parts of [src/camera/Camera.cpp](../src/camera/Camera.cpp). Everything below cost
real debugging time.

## The core radius moves the whole tree

The innermost ring has radius `coreRadius_` (at least 8192). After every rearrange, `arrange()`
checks the root's `subtree_arc_width`: the angle all its subfolders need. If it is over 225°, the
core grows by 1.25x and everything is laid out again further out, where the same platforms need
less angle; under 90°, it shrinks. So expanding a big folder can push every platform outward by a
lot. Fully expanding six open-source projects (825 folders) took the core from 8,192 to about
186,000.

Two things follow.

**Everything that caches a TreeV position goes stale when the core changes.** The camera is the
one that bit: its target is an absolute radius. When the core grew from 8,192 to 186,000, the
camera kept looking at r ≈ 10,000, inside the empty core, and the view went blank. `arrange()`
now moves the camera target by the same amount the core moved. It has to use
`MorphEngine::shift`, not `+=`: at startup the camera is mid-swoop, and the running morph would
overwrite a plain `+=` with its old end value every frame. `shift` moves the value and the
morph's start and end together.

**The grow loop only converges if every width depends on the radius.** Platform widths come from
`reshapePlatform(dnode, r0)`, which scales with 1/r. But a folder TreeV considers a *leaf*
(collapsed) keeps whatever `arc_width` it had, and still contributes `deployment * arc_width` to
its parent. If such a folder is deploying (`deployment > 0`) while marked collapsed, its width
never shrinks, the total never drops under 225°, and the loop grows the core until it overflows:

```
DBG arrange#71 core=8192 subtree=142.086 nan=0
DBG arrange#81 core=8.88034e+74 subtree=nan nan=19
```

Once it's NaN, nothing draws. The user sees the ring appear, expand, and vanish.

That is exactly what **Expand All** did: `CollapseExpand::execute(..., ExpandRecursive)` morphed
every folder's deployment to 1, but only marked the first two levels expanded in the tree panel,
and `GeometryManager::treevIsLeaf` reads the tree panel. The fix makes the panel update recursive
(`setTreeExpandedRecursive` in [CollapseExpand.cpp](../src/geometry/CollapseExpand.cpp)). The grow
loop is also capped at 64 steps (1.25^64 ≈ 1.6 million times the core). If widths ever stop
responding again, platforms overlap instead of the view vanishing.

MapV never had this problem: it doesn't read the tree panel to decide what's a leaf.

## Always use absolute coordinates

`treevGeom.platform.theta` and `.depth` are relative to the parent. Camera code that used them
directly aimed at the wrong place. Use `GeometryManager::treevPlatformR0(node)` and
`treevPlatformTheta(node)`, which sum up the ancestors. This applies to `treevLookAt`,
`treevLpanLookAt`, labels and hit testing alike.

## The root skips a ring

The metanode (the invisible parent of the scanned folder) would waste the first ring, since it
always has exactly one child. TreeV starts arranging and drawing from the scanned folder at
`coreRadius_ + PLATFORM_SPACING_DEPTH`, and forces its `arc_width` to the full 225° so its
children fill the ring.

## Camera up vector

`glm::lookAt` returns garbage (NaNs) when the up vector is parallel to the view direction; that
blanked the old DiscV view, which looked straight down. MapV and TreeV use up = +Z. Bird's eye
(`Camera::birdseyeView`) pitches to phi = 90°, straight down, and gets away with it only because
`cos(90°)` is about 6e-17 in floating point, not 0, and the view matrix is built in doubles. Any
new top-down view should keep phi just under 90° or pick another up vector.

## Framing a folder is close-up by design

`treevLookAt` on a folder frames that folder's own platform, as fsv did. For the root of a
fully expanded big tree, that means landing low against the inner ring, with the subfolders
toward the top of the screen. The README screenshot and GIF use the overview from earlier in the
swoop for that reason. Framing the whole subtree instead is in [ROADMAP.md](../ROADMAP.md).
