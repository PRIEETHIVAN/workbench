#pragma once
/**
 * backprop.hpp
 * Scalar neural-network backpropagation with gradient checking.
 *
 * Network architecture (single sample):
 *   z     = w*x + b          (pre-activation)
 *   h     = tanh(z)          (hidden activation)
 *   y_hat = v*h + c          (output)
 *   J     = 0.5*(y_hat - y)² (MSE loss, factor 1/2 for clean gradient)
 *
 * Training target: x=1, y_target=2
 * All maths uses std::tanh; no external libraries needed.
 */

#include <string>
#include <utility>
#include <vector>

namespace backprop {

/// Learnable parameters of the one-hidden-unit network
struct BackpropParams {
    double w;  ///< Input-to-hidden weight
    double b;  ///< Hidden bias
    double v;  ///< Hidden-to-output weight
    double c;  ///< Output bias
};

/// Values computed during the forward pass
struct ForwardResult {
    double z;      ///< Pre-activation: w*x + b
    double h;      ///< Hidden activation: tanh(z)
    double y_hat;  ///< Network output: v*h + c
    double loss;   ///< MSE loss: 0.5*(y_hat - y)²
};

/// Analytic gradients from backpropagation
struct GradResult {
    double dJ_dw;  ///< ∂J/∂w
    double dJ_db;  ///< ∂J/∂b
    double dJ_dv;  ///< ∂J/∂v
    double dJ_dc;  ///< ∂J/∂c
};

/// One row of gradient-check output (analytic vs. numeric)
struct GradCheckResult {
    std::string param_name;    ///< "w", "b", "v", or "c"
    double      analytic_grad; ///< from backward_pass()
    double      numeric_grad;  ///< central finite difference
    double      abs_diff;      ///< |analytic - numeric|
};

// ─── Core API ─────────────────────────────────────────────────────────────────

/**
 * Run the forward pass and compute loss.
 * @param p        Current parameter values
 * @param x        Input scalar
 * @param y_target Target scalar
 */
ForwardResult forward_pass(BackpropParams p, double x, double y_target);

/**
 * Compute analytic gradients via chain rule.
 *
 * Chain rule steps:
 *   dJ/dy_hat = y_hat - y
 *   dJ/dc     = dJ/dy_hat · 1
 *   dJ/dv     = dJ/dy_hat · h
 *   dJ/dh     = dJ/dy_hat · v
 *   dJ/dz     = dJ/dh     · (1 - h²)   [d/dz tanh(z) = 1 - tanh²(z)]
 *   dJ/dw     = dJ/dz     · x
 *   dJ/db     = dJ/dz     · 1
 *
 * @param p        Parameters used for the forward pass
 * @param fr       Result of forward_pass(p, x, y_target)
 * @param x        Same input scalar used in forward_pass
 * @param y_target Same target used in forward_pass
 */
GradResult backward_pass(BackpropParams p, ForwardResult fr,
                         double x, double y_target);

/**
 * Apply one gradient-descent update step.
 *   p_new = p - lr * grad
 *
 * @param p   Current parameters
 * @param gr  Gradients from backward_pass()
 * @param lr  Learning rate
 * @return    Updated parameters
 */
BackpropParams gradient_descent_step(BackpropParams p, GradResult gr, double lr);

/**
 * Verify analytic gradients against central finite differences.
 *
 *   numeric_grad(param) = [J(param + ε) - J(param - ε)] / (2ε)
 *
 * @param p        Parameter point to check at
 * @param x        Input scalar
 * @param y_target Target scalar
 * @param epsilon  Perturbation size (default 1e-6)
 * @return         One GradCheckResult per parameter {w, b, v, c}
 */
std::vector<GradCheckResult> check_gradients(BackpropParams p,
                                              double x, double y_target,
                                              double epsilon = 1e-6);

/**
 * Train the network for `steps` gradient-descent iterations.
 *
 * @param init      Initial parameter values
 * @param x         Input scalar (fixed throughout training)
 * @param y_target  Target scalar
 * @param lr        Learning rate
 * @param steps     Number of gradient-descent steps (default 100)
 * @return          {step_index, loss} pairs for every step (0-indexed)
 */
std::vector<std::pair<int,double>> train(BackpropParams init,
                                          double x, double y_target,
                                          double lr, int steps = 100);

} // namespace backprop
