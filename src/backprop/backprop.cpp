/**
 * backprop.cpp
 * Implementation of scalar backpropagation and gradient checking.
 */

#include "backprop.hpp"

#include <cmath>
#include <stdexcept>

namespace backprop {

// ─── Forward pass ─────────────────────────────────────────────────────────────
ForwardResult forward_pass(BackpropParams p, double x, double y_target) {
    ForwardResult fr;
    fr.z     = p.w * x + p.b;
    fr.h     = std::tanh(fr.z);
    fr.y_hat = p.v * fr.h + p.c;
    fr.loss  = 0.5 * (fr.y_hat - y_target) * (fr.y_hat - y_target);
    return fr;
}

// ─── Backward pass (chain rule) ───────────────────────────────────────────────
GradResult backward_pass(BackpropParams p, ForwardResult fr,
                          double x, double y_target) {
    // Upstream gradient from loss
    double dJ_dyhat = fr.y_hat - y_target;      // dJ/d(y_hat)

    // Output layer
    double dJ_dc = dJ_dyhat * 1.0;              // dJ/dc  = dJ/dy_hat * dc/dc = 1
    double dJ_dv = dJ_dyhat * fr.h;             // dJ/dv  = dJ/dy_hat * h

    // Hidden layer
    double dJ_dh = dJ_dyhat * p.v;              // dJ/dh  = dJ/dy_hat * v
    double dJ_dz = dJ_dh * (1.0 - fr.h * fr.h); // dJ/dz = dJ/dh * (1 - tanh²(z))

    // Input layer
    double dJ_dw = dJ_dz * x;                  // dJ/dw  = dJ/dz * x
    double dJ_db = dJ_dz * 1.0;                // dJ/db  = dJ/dz

    return { dJ_dw, dJ_db, dJ_dv, dJ_dc };
}

// ─── Gradient descent step ────────────────────────────────────────────────────
BackpropParams gradient_descent_step(BackpropParams p, GradResult gr, double lr) {
    p.w -= lr * gr.dJ_dw;
    p.b -= lr * gr.dJ_db;
    p.v -= lr * gr.dJ_dv;
    p.c -= lr * gr.dJ_dc;
    return p;
}

// ─── Gradient check (central finite differences) ──────────────────────────────
std::vector<GradCheckResult> check_gradients(BackpropParams p,
                                              double x, double y_target,
                                              double epsilon) {
    // First compute analytic gradients
    ForwardResult fr = forward_pass(p, x, y_target);
    GradResult    gr = backward_pass(p, fr, x, y_target);

    auto num_grad = [&](double& param_ref) -> double {
        double saved = param_ref;
        param_ref = saved + epsilon;
        double jp = forward_pass(p, x, y_target).loss;
        param_ref = saved - epsilon;
        double jm = forward_pass(p, x, y_target).loss;
        param_ref = saved;
        return (jp - jm) / (2.0 * epsilon);
    };

    std::vector<GradCheckResult> results;
    results.reserve(4);

    // w
    {
        double ng = num_grad(p.w);
        results.push_back({"w", gr.dJ_dw, ng, std::abs(gr.dJ_dw - ng)});
    }
    // b
    {
        double ng = num_grad(p.b);
        results.push_back({"b", gr.dJ_db, ng, std::abs(gr.dJ_db - ng)});
    }
    // v
    {
        double ng = num_grad(p.v);
        results.push_back({"v", gr.dJ_dv, ng, std::abs(gr.dJ_dv - ng)});
    }
    // c
    {
        double ng = num_grad(p.c);
        results.push_back({"c", gr.dJ_dc, ng, std::abs(gr.dJ_dc - ng)});
    }

    return results;
}

// ─── Training loop ────────────────────────────────────────────────────────────
std::vector<std::pair<int,double>> train(BackpropParams init,
                                          double x, double y_target,
                                          double lr, int steps) {
    if (steps < 1) throw std::invalid_argument("steps must be >= 1");

    std::vector<std::pair<int,double>> history;
    history.reserve(static_cast<size_t>(steps) + 1);

    BackpropParams p = init;
    history.emplace_back(0, forward_pass(p, x, y_target).loss);
    for (int s = 1; s <= steps; ++s) {
        ForwardResult fr = forward_pass(p, x, y_target);
        GradResult gr = backward_pass(p, fr, x, y_target);
        p = gradient_descent_step(p, gr, lr);
        history.emplace_back(s, forward_pass(p, x, y_target).loss);
    }
    return history;
}

} // namespace backprop
