# ADR-005: Optical distortion approximation
Context: Optical soundtrack distortion must be audibly/measurably distinct from generic clippers: HF-selective intermod (sibilance splatter), peak rounding, VA/VD asymmetry differences, level-dependent HF loss (image spread).
Decision: MVP chain = slit-loss LPF → HF pre-emphasis → mode-dependent sigmoid (VA symmetric / VD asymmetric gamma) with fast peak-rounding gain stage → de-emphasis → envelope-driven level-dependent LPF (image-spread approximation) behind `IImageSpreadStage`; 2×/4× oversampling around the shaper; DC servo.
Alternatives: physical exposure/development/slit convolution model (post-MVP, interface reserved); waveshaper only (fails distinctness requirement — rejected).
Consequences: Distinctness is objectively testable (THD@8k ≫ THD@200 Hz; negative HF-vs-level correlation).
Validation: DSP/OpticalDistinctnessTest.
Reversal cost: Low — interface isolation.
