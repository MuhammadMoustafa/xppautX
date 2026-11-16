// The Morris-Lecar cable of examples/ode/lecdiff200.odex (the model of lecar.odex with diffusive coupling), one thread per cell.
// The cell's right-hand side is the file's lines 10-19, in the order written there:
//   fun minf(v) = 0.5*(1+tanh((v-va)/ode_divisor(vb)))   fun ninf(v) = 0.5*(1+tanh((v-vc)/ode_divisor(vd)))
//   fun lamn(v) = phi*cosh((v-vc)/ode_divisor(2*vd))      fun ica(v) = gca*minf(v)*(v-1)
//   fun f(v, w) = iapp+gl*(vl-v)+gk*w*(vk-v)-ica(v)       fun g(v, w) = lamn(v)*(ninf(v)-w)
//   v0 = v1   v<n+1> = v<n>   v[j]' = f(v[j], w[j])+dif*(v[j+1]-2*v[j]+v[j-1])   w[j]' = g(v[j], w[j])
// State layout: v[0..n), then w[0..n) (the order of the file's variables). One RK4 step (core/odesol2.cpp's one_step_rk4) is four
// kernel launches, stage(1..4), because a cell reads its neighbours' stage values: acc is the running sum, yin the stage input, yout the next one.
// The tanh and cosh used are the ones q3_network.py puts in (crd_tanh/crd_cosh: CORE-MATH's, or tanh/cosh: CUDA's), and the file's `par` values.
@PARAMS@

__device__ __forceinline__ double ode_divisor(double value) { return value != 0.0 ? value : 2.23e-15; }

extern "C" __global__ void stage(int which, int n, double dt, double *y, double *acc, const double *yin, double *yout) {
    int j = blockIdx.x * blockDim.x + threadIdx.x;
    if (j >= n) return;
    const double vj = yin[j], wj = yin[n + j];
    const double vm = yin[j == 0 ? 0 : j - 1], vp = yin[j == n - 1 ? n - 1 : j + 1];
    double minf = 0.5 * (1 + @TANH@((vj - va) / ode_divisor(vb)));
    double ninf = 0.5 * (1 + @TANH@((vj - vc) / ode_divisor(vd)));
    double lamn = phi * @COSH@((vj - vc) / ode_divisor(2 * vd));
    double ica = gca * minf * (vj - 1);
    double f = iapp + gl * (vl - vj) + gk * wj * (vk - vj) - ica;
    double kv = f + dif * (vp - 2 * vj + vm);
    double kw = lamn * (ninf - wj);
    if (which == 1) {
        acc[j] = y[j] + dt * kv / 6.00;  acc[n + j] = y[n + j] + dt * kw / 6.00;
        yout[j] = y[j] + dt * kv * 0.5;  yout[n + j] = y[n + j] + dt * kw * 0.5;
    } else if (which == 2) {
        acc[j] = acc[j] + dt * kv / 3.00;  acc[n + j] = acc[n + j] + dt * kw / 3.00;
        yout[j] = y[j] + .5 * dt * kv;     yout[n + j] = y[n + j] + .5 * dt * kw;
    } else if (which == 3) {
        acc[j] = acc[j] + dt * kv / 3.000;  acc[n + j] = acc[n + j] + dt * kw / 3.000;
        yout[j] = y[j] + dt * kv;           yout[n + j] = y[n + j] + dt * kw;
    } else {
        y[j] = acc[j] + dt * kv / 6.00;  y[n + j] = acc[n + j] + dt * kw / 6.00;
    }
}
