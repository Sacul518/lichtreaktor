# LED placement

The numbers aren't random, even if typing them into KiCad feels a bit like it. The board has a 45 mm radius; the LED centres follow a smaller circle with a 38 mm radius. Its centre is at $(x_c,y_c)=(185,65)\,\mathrm{mm}$ in the PCB editor, right on the encoder shaft.

The planned ring starts 30 degrees clockwise from the top, with 13 degrees between neighbouring LEDs. For LED number $n$, from 1 to 24:

$$
\theta_n = 30^\circ + (n-1)\cdot 13^\circ
$$

The last LED lands at 329 degrees. That leaves a 61-degree gap across the top for the XIAO and USB connection. These angles are layout choices; the circle formula gives the coordinates from them.

KiCad's Y coordinate increases downwards. Measuring the angle clockwise from the top gives:

$$
x_{\mathrm{centre},n}=185\,\mathrm{mm}+38\,\mathrm{mm}\cdot\sin(\theta_n)
$$

$$
y_{\mathrm{centre},n}=65\,\mathrm{mm}-38\,\mathrm{mm}\cdot\cos(\theta_n)
$$

One small catch: the `LED_THT:LED_D5.0mm` footprint is anchored on pad 1, with the LED centre 1.27 mm to its right at 0-degree rotation. So the positions entered in KiCad are:

$$
x_{\mathrm{KiCad},n}=x_{\mathrm{centre},n}-1.27\,\mathrm{mm},\qquad
y_{\mathrm{KiCad},n}=y_{\mathrm{centre},n}
$$

For D1, $\theta_1=30^\circ$, which gives:

$$
x_{\mathrm{KiCad},1}=185+38\sin(30^\circ)-1.27=202.73\,\mathrm{mm}
$$

$$
y_{\mathrm{KiCad},1}=65-38\cos(30^\circ)\approx32.09\,\mathrm{mm}
$$

All LEDs use 0-degree rotation for this calculation, and coordinates are rounded to 0.01 mm. Neighbouring LED centres are about $2\cdot38\sin(13^\circ/2)=8.60\,\mathrm{mm}$ apart. Some resistor positions were adjusted by hand to avoid overlaps; the final layout passes KiCad's copper-clearance and courtyard checks.

[Back to the project README](../README.md)
