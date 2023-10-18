# Focus sequencing module

## Purpose

*Focus sequencing* is a [Magic Lantern](https://magiclantern.fm/) **module** helping to solve the following situations:

- We cannot use an external *focus puller monitor* because we're recording videos using **crop modes**, which are currently incompatible with the camera *HDMI output*
- We're recording videos as a **single camera operator** without the assistance of a *focus puller* and the sequence is too complex to deal with *manual focusing*

👉 *focus sequencing* provides then a way to easily **record multiple lens focus points**, so we can prepare a sequence including camera or target movements and subsequently **replay** them as we're recording videos, using a simple push button to switch to a point to the next one in the sequence.

> **Typical use case**: I'm a *single camera operator* recording a video using *crop modes* and my sequence is a combination of a *dolly-in* movement (with a speed ramp) and a horizontal camera rotation used to keep an object at the center of the composition.
>
> 👉 While I'm rehearsing the camera track and movements, I'll record three or four intermediate focus points in a sequence with proper speed transition settings, so my targeted object is always perfectly focused and I'll replay the focus sequence while I'm finally recording the scene, going forward from a focus point to the next one by pushing a single button.

## Technical issues and workarounds

### State of the art

Imagine you want to play around focus points with a given camera lens, using *Magic Lantern* you can see that:

- we can move the lens programmatically, using the ***lens_focus*** function available in *lens.c*

  > to do so we basically ask the camera to do a lens rotation of given **step size**, that can be 1, 2 or 3, *positive* or *negative* to move the rotor *forward* or *backward*

- the lens is able to report **raw lens positions**

- the lens is able to report **focus distances** (in cm)

### Issue with Magic Lantern

The first issue to solve is related to the *lens_focus* function itself and its link to the reported *raw lens position* and *focus distance*, that are accessible by reading values in the exposed *lens_info* structure:

When moving the lens - if we ask to do so (using the *wait* parameter) - we expect naturally the function to **wait** after the move until the values become readable, but in practice it's **barely the case** as there's a potential delay between the lens rotation *confirmation flag* set by the camera (basically just indicating if the *rotation is ok* or if *something went wrong* during the move - like *reaching the lens limits*) and the update of the *lens_info* structure content itself, that may require multiple updates calls before being properly ***stabilized***, particularly when dealing with *step sizes* of 2 and 3.

This is very problematic as the *lens_focus* method become unreliable when we expect to correlate a lens move rotation query with a lens positioning feedback.

To solve this issue, a new ***lens_focus_ex*** function was added in *lens.c* (hence the specific *ML build*) with a clearer API (number of passes in the rotation *loop*, *step size* value, lens rotation direction - *forward* or *backward*) including a ***wait for feedback*** explicit and **reliable** flag that guarantee both the *raw lens positions* and *focus distances* values in the *lens_info* structure will be fully stabilized and readable after the call.

### Issue with lens specificities

Now, let's consider one specific camera lens for our tests: the **Canon EF 24mm f/2.8 IS USM**.

First of all, by reaching the extreme first rotation position of the lens then moving it *step by step* using our new *lens_focus_ex* function, we can report we got **407 mechanical steps** between the limits.

> - ❌ this number is specific to the lens
> - ✅ the step progression is obviously **increasing** and **linear**

👉 We need to **evaluate the mechanical steps** for each lens.

For each step, we can now report the corresponding **raw lens position** values by reading the *lens_info* structure after each function call:

![](readme.assets/1_raw_positions.png)

> These *raw lens positions* are:
> - ✅ **precise** enough: one specific value per mechanical step
> - ❌ **not absolute**: depends on the lens position at camera startup (e.g.: [2841,-2373] value range)
> - ❌ increasing or decreasing, but **not following a linear progression** (more obvious when using a logarithmic scale)
> - ❌ **not necessarily progressing forward** regarding mechanical step progression (e.g.: *inverted* here)

And also the corresponding **reported focus distances** in cm:

![](readme.assets/2_reported_distances.png)

> These *focus distances* being:
> - ❌ **not precise** at all, with the same value reported for ~10 mechanical steps
> - ✅ **absolutes**: with always the same range (e.g.: [1647,20] value range here, 65535 meaning *infinite focus*)
> - ❌ increasing or decreasing, but **not following a linear progression**
> - ❌ **not necessarily progressing forward** regarding mechanical step progression (e.g.: inverted here)

👉 We need then to systematically determine the **minimal and maximal *raw lens positions* values** and **progression sign**, so we can compute corresponding **normalized lens positions**:

![](readme.assets/3_normalized_positions.png)

> These *normalized lens positions* being:
> - ✅ **precise** enough: one specific value per mechanical step
> - ✅ **absolutes**: with always the same value range (e.g.: [0,5214] value range here)
> - ❌ increasing or decreasing, but **not following a linear progression**
> - ✅ **progressing forward**

👉 We can save the result of the **lens calibration** process performed when we detect a new lens, that will include the **specific number of mechanical steps** and the corresponding **normalized lens position** for each step.

When restarting the camera, if the lens doesn't change, we can reload this calibration result and just determine the current **minimal and maximal *raw lens positions* values** which is potentially specific for each run, so we may be able to use the result of an **autofocus operation** by computing the corresponding **normalized lens position**.

Each focus points being systematically stored using *normalized lens positions*, we just have to determine the ✅ **closest mechanical step positions** corresponding to the *source* and *destination* *normalized lens values* then we can move the lens rotor accordingly to go from a focus point to the other.

The *focus distances* being always absolute, we can display it indicatively (because of the precision issue).

### Issue with *step size* lens rotation *steps*

This work perfectly when only moving the lens rotor using **step sizes of 1**, but when using a **step size of 2** and **3** we can see the **effective rotor lens step** performed by the camera are respectively of **4** and **26 steps** (averaged).

> ❌ the correlation between the *step size* value and the effective rotor *lens step* **is not linear** and specific per lens (and camera?)

### Issue with *step size* lens rotation *speeds*

The same issue occurs when checking the related speeds per queried *step sizes*:

- a lens *step size* of **1** will be executed at an average speed of **23.496 steps/s**
- a lens *step size* of **2** will be executed at an average speed of **48.733 steps/s**
- a lens *step size* of **3** will be executed at an average speed of **94.321 steps/s**

> ❌ the correlation between the *step size* value and the effective *lens rotation speed* **is not linear** and specific per lens (and camera?)

👉 We need then to evaluate both averaged **effective rotor lens step** and **rotation speed** for each possible ***step size*** value.

When doing a *lens focus transition* operation using a target duration, we can now compute a proper ✅ **distribution of the different *step size* and *wait calls*** so we may **reach *exactly* the target position** with a **duration *as close as possible* as queried** by the user using the **smoothest focus transition** possible, by taking in account both the *effective rotor lens* stepping and the *step size speeds*.

## How to install

Because we rely over a specific *lens_focus* function, there's currently no way to use this module outside specific builds that are including this function.

Our best shot is then to use one specific ***crop_rec_4k_mlv_snd_isogain_1x3_presets_ultrafast_fsq*** *Magic Lantern* build [available in my personal repository](https://github.com/arnaud-sintes/magiclantern_asintes/releases), which is basically an up-to-date fork of Danne's one (with also the *[ultrafast framed preview](https://www.magiclantern.fm/forum/index.php?topic=26998)* feature).

Using this build, the ***focus_sq*** module is simply **embedded and activated by default**.

## How to use

TODO

### Toggle between modes

### Calibration

### Edit focus sequence

### Replay focus sequence

### Battery saving

### Save and reload

## Summary