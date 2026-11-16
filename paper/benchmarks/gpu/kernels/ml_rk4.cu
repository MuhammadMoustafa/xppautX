// Morris-Lecar (Koch & Segev's chapter; examples/ode/lecar.odex), fixed-step RK4, one run per thread over a list of one parameter.
// The right-hand side is lecar.odex's lines 14-21, in the order they are written there:
//   fun minf(v) = 0.5*(1+tanh((v-v1)/ode_divisor(v2)))        fun ninf(v) = 0.5*(1+tanh((v-v3)/ode_divisor(v4)))
//   fun lamn(v) = phi*cosh((v-v3)/ode_divisor(2*v4))           ica = gca*minf(v)*(v-1)
//   v' = iapp+gl*(vl-v)+gk*w*(vk-v)-ica                        w' = lamn(v)*(ninf(v)-w)
//   fun ode_divisor(value) = if value then value else 2.23e-15
// The RK4 step is core/odesol2.cpp's one_step_rk4, operation by operation (y+dt*k/6.00 is (dt*k)/6, .5*dt*k is (.5*dt)*k, t += dt).
// The tanh and cosh used are the ones q2_sweep.py puts in (crd_tanh/crd_cosh: CORE-MATH's, or tanh/cosh: CUDA's), and the file's `par` values.
@PARAMS@

__device__ __forceinline__ double ode_divisor(double value) { return value != 0.0 ? value : 2.23e-15; }

__device__ __forceinline__ void rhs(double iapp, double v, double w, double &dv, double &dw) {
    double minf = 0.5 * (1 + @TANH@((v - v1) / ode_divisor(v2)));
    double ninf = 0.5 * (1 + @TANH@((v - v3) / ode_divisor(v4)));
    double lamn = phi * @COSH@((v - v3) / ode_divisor(2 * v4));
    double ica = gca * minf * (v - 1);
    dv = iapp + gl * (vl - v) + gk * w * (vk - v) - ica;
    dw = lamn * (ninf - w);
}

// out[3*i] = t, out[3*i+1] = v, out[3*i+2] = w after nsteps steps of run i; run i < ntraj also stores every row (t, v, w) in traj.
extern "C" __global__ void ml_rk4(const double *iapp_values, long long n, int nsteps, double dt, double *out, double *traj, int ntraj) {
    long long i = (long long)blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n) return;
    const double iapp = iapp_values[i];
    double t = 0.0, v = V0, w = W0;
    double *row = (traj != nullptr && i < ntraj) ? traj + i * (long long)(nsteps + 1) * 3 : nullptr;
    if (row) { row[0] = t; row[1] = v; row[2] = w; }
    for (int s = 0; s < nsteps; s++) {
        double k1v, k1w, k2v, k2w, k3v, k3w, k4v, k4w;
        rhs(iapp, v, w, k1v, k1w);
        double av = v + dt * k1v / 6.00, aw = w + dt * k1w / 6.00;
        double yv = v + dt * k1v * 0.5, yw = w + dt * k1w * 0.5;
        rhs(iapp, yv, yw, k2v, k2w);
        av = av + dt * k2v / 3.00; aw = aw + dt * k2w / 3.00;
        yv = v + .5 * dt * k2v; yw = w + .5 * dt * k2w;
        rhs(iapp, yv, yw, k3v, k3w);
        av = av + dt * k3v / 3.000; aw = aw + dt * k3w / 3.000;
        yv = v + dt * k3v; yw = w + dt * k3w;
        rhs(iapp, yv, yw, k4v, k4w);
        v = av + dt * k4v / 6.00; w = aw + dt * k4w / 6.00;
        t = t + dt;
        if (row) { row[3 * (s + 1)] = t; row[3 * (s + 1) + 1] = v; row[3 * (s + 1) + 2] = w; }
    }
    out[3 * i] = t; out[3 * i + 1] = v; out[3 * i + 2] = w;
}
