# Backpropagation hand-work appendix (content draft)

This is a legible calculation draft for the required handwritten page(s); it is not a scan or a claim of handwriting. The candidate should independently copy/check the derivation by hand and attach their own scan or photo to the submission.

Given x=1, y=2, w=0.5, b=0, v=1, c=0, and η=0.1:

- z=wx+b=0.5; h=tanh(0.5)=0.4621171573.
- ŷ=vh+c=0.4621171573; e=ŷ−y=−1.5378828427.
- J=0.5e²=1.1825455724.
- dJ/dŷ=e=−1.5378828427; dJ/dc=e=−1.5378828427.
- dJ/dv=e h=−0.7106501366.
- dJ/dz=e v (1−h²)=−1.2092861852.
- dJ/dw=(dJ/dz)x=−1.2092861852; dJ/db=dJ/dz=−1.2092861852.

Use these original-parameter gradients simultaneously:

- w_new=0.5−0.1(−1.2092861852)=0.6209286185.
- b_new=0−0.1(−1.2092861852)=0.1209286185.
- v_new=1−0.1(−0.7106501366)=1.0710650137.
- c_new=0−0.1(−1.5378828427)=0.1537882843.

Each gradient is negative, so subtracting the gradient increases its parameter. The update moves the prediction toward the target and reduces the local loss. For a final submission, hand-copy this derivation, attach a dated scan/photo, and independently compare the numbers against the C++ finite-difference output.
