"""W364 (#426): independent analytic derivatives for the reaction Hopf curve.
No AUTO code or finite differences. q has unit Hermitian norm, <p,q>=1.
Run with python tests/auto/codim_coefficients.py (through tools/heavy.sh).
"""
import math

def l1(t):
    x = t / (1+t)
    p1 = (4+t)*(1+t)/t
    omega = math.sqrt(2*t-1)
    jacobian = ((-1-t, x), (-p1*t, 1+t))
    q = [complex(x), complex(1+t, omega)]
    norm = math.sqrt(sum(abs(v)**2 for v in q))
    q = [v/norm for v in q]
    p = [complex(-p1*t), complex(1+t, -omega)]
    alpha = sum(v.conjugate()*z for v, z in zip(p, q))
    p = [v/alpha.conjugate() for v in p]
    conjugate_q = [v.conjugate() for v in q]

    def quadratic(q, r):
        value = -t*(q[0]*r[1]+q[1]*r[0])+x*q[1]*r[1]
        return [value, p1*value]

    def cubic(q, r, s):
        value = -t*(q[0]*r[1]*s[1]+q[1]*r[0]*s[1]+q[1]*r[1]*s[0])+x*q[1]*r[1]*s[1]
        return [value, p1*value]

    def solve(a, b):
        # Independent exact 2x2 formula, with no AUTO linear algebra.
        determinant = a[0][0]*a[1][1]-a[0][1]*a[1][0]
        return [(a[1][1]*b[0]-a[0][1]*b[1])/determinant,
                (-a[1][0]*b[0]+a[0][0]*b[1])/determinant]

    r = solve(jacobian, quadratic(q, conjugate_q))
    shifted = tuple(tuple((2j*omega if i == j else 0)-jacobian[i][j]
                          for j in range(2)) for i in range(2))
    s = solve(shifted, quadratic(q, q))
    one = cubic(q, q, conjugate_q)
    two = quadratic(q, r)
    three = quadratic(conjugate_q, s)
    return sum(p[i].conjugate()*(one[i]-2*two[i]+three[i]) for i in range(2)).real/(2*omega)


for t in (.51, 1, 2, 3, 4, 5, 5.5, 6, 7, 8, 10, 20):
    coefficient = l1(t)
    # The independent planar formula in auto07p-findings.md has this sign.
    assert coefficient * (24-10*t-8*t*t-t*t*t) > 0
    print(t, t*math.exp(-(4+t)/3), coefficient)
