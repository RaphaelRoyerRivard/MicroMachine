#include "influence.h"
#include <cmath>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <random>

using namespace std;
using namespace sc2;

InfluenceMap::InfluenceMap(const sc2::ImageData map)
    : InfluenceMap(map.width, map.height) {
    assert(map.bits_per_pixel == 8);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            weights[y * w + x] = (uint8_t)map.data[(h - y - 1) * w + x] == 255 ? 0.0 : 1.0;
        }
    }
}

InfluenceMap::InfluenceMap(const SC2APIProtocol::ImageData map)
    : InfluenceMap(map.size().x(), map.size().y()) {
    auto& data = map.data();
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            weights[y * w + x] = (uint8_t)data[(h - y - 1) * w + x];
        }
    }
}

static pair<int, int> round_point(Point2D p) {
    return make_pair((int)round(p.x), (int)round(p.y));
}

InfluenceMap& InfluenceMap::operator+=(const InfluenceMap& other) {
    for (int i = 0; i < w * h; i++) {
        weights[i] += other.weights[i];
    }
    return (*this);
}

InfluenceMap& InfluenceMap::operator+=(double other) {
    for (int i = 0; i < w * h; i++) {
        weights[i] += other;
    }
    return (*this);
}

InfluenceMap InfluenceMap::operator+(const InfluenceMap& other) const {
    auto ret = (*this);
    return (ret += other);
}

InfluenceMap& InfluenceMap::operator-=(const InfluenceMap& other) {
    assert(w == other.w);
    assert(h == other.h);

    for (int i = 0; i < w * h; i++) {
        weights[i] -= other.weights[i];
    }
    return (*this);
}

InfluenceMap InfluenceMap::operator-(const InfluenceMap& other) const {
    auto ret = (*this);
    return (ret -= other);
}

InfluenceMap& InfluenceMap::operator*=(const InfluenceMap& other) {
    assert(w == other.w);
    assert(h == other.h);
    for (int i = 0; i < w * h; i++) {
        weights[i] *= other.weights[i];
    }
    return (*this);
}

InfluenceMap InfluenceMap::operator*(const InfluenceMap& other) const {
    auto ret = (*this);
    return (ret *= other);
}

InfluenceMap& InfluenceMap::operator/=(const InfluenceMap& other) {
    assert(w == other.w);
    assert(h == other.h);

    for (int i = 0; i < w * h; i++) {
        weights[i] /= other.weights[i];
    }
    return (*this);
}

InfluenceMap InfluenceMap::operator/(const InfluenceMap& other) const {
    auto ret = (*this);
    return (ret /= other);
}

InfluenceMap InfluenceMap::operator+(double factor) const {
    InfluenceMap ret = InfluenceMap(w, h);
    for (int i = 0; i < w * h; i++) {
        ret.weights[i] = weights[i] + factor;
    }
    return ret;
}

InfluenceMap InfluenceMap::operator-(double factor) const {
    InfluenceMap ret = InfluenceMap(w, h);
    for (int i = 0; i < w * h; i++) {
        ret.weights[i] = weights[i] - factor;
    }
    return ret;
}

InfluenceMap InfluenceMap::operator*(double factor) const {
    InfluenceMap ret = InfluenceMap(w, h);
    for (int i = 0; i < w * h; i++) {
        ret.weights[i] = weights[i] * factor;
    }
    return ret;
}

void InfluenceMap::operator*=(double factor) {
    for (int i = 0; i < w * h; i++) {
        weights[i] *= factor;
    }
}

void InfluenceMap::threshold(double value) {
    for (int i = 0; i < w * h; i++)
        weights[i] = weights[i] >= value ? 1 : 0;
}

double InfluenceMap::sum() const {
    double ret = 0.0;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            ret += weights[y * w + x];
        }
    }
    return ret;
}

void InfluenceMap::max(const InfluenceMap& other) {
    for (int i = 0; i < w*h; i++) {
        weights[i] = std::max(weights[i], other.weights[i]);
    }
}

double InfluenceMap::max() const {
    double ret = 0.0;
    for (int i = 0; i < w * h; i++) {
        ret = std::max(ret, weights[i]);
    }
    return ret;
}

double InfluenceMap::maxFinite() const {
    double ret = 0.0;
    for (auto w : weights) {
        if (isfinite(w))
            ret = std::max(ret, w);
    }
    return ret;
}

Point2DI InfluenceMap::argmax() const {
    double mx = -100000;
    Point2DI best(0, 0);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            if (weights[y * w + x] > mx && isfinite(weights[y * w + x])) {
                mx = weights[y * w + x];
                best = Point2DI(x, y);
            }
        }
    }
    return best;
}

InfluenceMap InfluenceMap::replace_nonzero(double with) const {
    InfluenceMap ret = InfluenceMap(w, h);
    for (int i = 0; i < w * h; i++) {
        ret.weights[i] = weights[i] != 0 ? with : 0;
    }
    return ret;
}

InfluenceMap InfluenceMap::replace_nan(double with) const {
    InfluenceMap ret = InfluenceMap(w, h);
    for (int i = 0; i < w * h; i++) {
        ret.weights[i] = isnan(weights[i]) ? with : weights[i];
    }
    return ret;
}

InfluenceMap InfluenceMap::replace(double value, double with) const {
    InfluenceMap ret = InfluenceMap(w, h);
    for (int i = 0; i < w * h; i++) {
        ret.weights[i] = weights[i] == value ? with : weights[i];
    }
    return ret;
}

void InfluenceMap::addInfluence(double influence, Point2DI pos) {
    assert(pos.x >= 0 && pos.x < w && pos.y >= 0 && pos.y < h);
    weights[pos.y * w + pos.x] += influence;
}

void InfluenceMap::addInfluence(double influence, Point2D pos) {
    auto p = round_point(pos);
    assert(p.first >= 0 && p.first < w && p.second >= 0 && p.second < h);
    weights[p.second * w + p.first] += influence;
}

void InfluenceMap::setInfluence(double influence, Point2D pos) {
    auto p = round_point(pos);
    assert(p.first >= 0 && p.first < w && p.second >= 0 && p.second < h);
    weights[p.second * w + p.first] = influence;
}

void InfluenceMap::addInfluenceInDecayingCircle(double influence, double radius, Point2D pos) {
    int x0, y0;
    tie(x0, y0) = round_point(pos);

    int r = (int)ceil(radius);
    for (int dx = -r; dx <= r; dx++) {
        for (int dy = -r; dy <= r; dy++) {
            int x = x0 + dx;
            int y = y0 + dy;
            if (x >= 0 && y >= 0 && x < w && y < h && dx * dx + dy * dy < radius * radius) {
                weights[y * w + x] = influence / (1 + dx * dx + dy * dy);
            }
        }
    }
}

void InfluenceMap::setInfluenceInCircle(double influence, double radius, Point2D pos) {
    int x0, y0;
    tie(x0, y0) = round_point(pos);

    int r = (int)ceil(radius);
    for (int dx = -r; dx <= r; dx++) {
        for (int dy = -r; dy <= r; dy++) {
            int x = x0 + dx;
            int y = y0 + dy;
            if (x >= 0 && y >= 0 && x < w && y < h && dx * dx + dy * dy < radius * radius) {
                weights[y * w + x] = influence;
            }
        }
    }
}

void InfluenceMap::addInfluence(const vector<vector<double> >& influence, Point2D pos) {
    int x0, y0;
    tie(x0, y0) = round_point(pos);

    int r = influence.size() / 2;
    for (int dx = -r; dx <= r; dx++) {
        for (int dy = -r; dy <= r; dy++) {
            int x = x0 + dx;
            int y = y0 + dy;
            if (x >= 0 && y >= 0 && x < w && y < h) {
                weights[y * w + x] += influence[dx + r][dy + r];
            }
        }
    }
}

void InfluenceMap::addInfluenceMultiple(const vector<vector<double> >& influence, Point2D pos, double factor) {
    int x0, y0;
    tie(x0, y0) = round_point(pos);

    int r = influence.size() / 2;
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            int x = x0 + dx;
            int y = y0 + dy;
            if (x >= 0 && y >= 0 && x < w && y < h) {
                weights[y * w + x] += influence[dx + r][dy + r] * factor;
            }
        }
    }
}

void InfluenceMap::maxInfluence(const vector<vector<double> >& influence, Point2D pos) {
    int x0, y0;
    tie(x0, y0) = round_point(pos);

    int r = influence.size() / 2;
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            int x = x0 + dx;
            int y = y0 + dy;
            if (x >= 0 && y >= 0 && x < w && y < h) {
                weights[y * w + x] = std::max(weights[y * w + x], influence[dx + r][dy + r]);
            }
        }
    }
}

void InfluenceMap::maxInfluenceMultiple(const vector<vector<double> >& influence, Point2D pos, double factor) {
    int x0, y0;
    tie(x0, y0) = round_point(pos);

    int r = influence.size() / 2;
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            int x = x0 + dx;
            int y = y0 + dy;
            if (x >= 0 && y >= 0 && x < w && y < h) {
                weights[y * w + x] = std::max(weights[y * w + x], influence[dx + r][dy + r] * factor);
            }
        }
    }
}

vector<double> temporary_buffer;
void InfluenceMap::propagateMax(double decay, double speed, const InfluenceMap& traversable) {
    double factor = 1 - decay;
    // Diagonal decay
    double factor2 = pow(factor, 1.41);

    temporary_buffer.resize(weights.size());
    vector<double>& newWeights = temporary_buffer;

    for (int y = 0; y < h; y++) {
        weights[y * w + 0] = weights[y * w + 1];
        weights[y * w + w - 1] = weights[y * w + w - 2];
    }
    for (int x = 0; x < w; x++) {
        weights[x] = weights[x + w];
        weights[(h - 1) * w + x] = weights[(h - 2) * w + x];
    }

    for (int y = 1; y < h - 1; y++) {
        int yw = y * w;
        for (int x = 1; x < w - 1; x++) {
            int i = yw + x;
            if (traversable[i] == 0) {
                newWeights[i] = 0;
                continue;
            }

            double c = 0;
            c = std::max(c, weights[i]);
            c = std::max(c, weights[i - 1]);
            c = std::max(c, weights[i + 1]);
            c = std::max(c, weights[i - w]);
            c = std::max(c, weights[i + w]);
            c *= factor;

            double c2 = 0;
            c2 = std::max(c2, weights[i - w - 1]);
            c2 = std::max(c2, weights[i - w + 1]);
            c2 = std::max(c2, weights[i + w - 1]);
            c2 = std::max(c2, weights[i + w + 1]);
            c2 *= factor2;
            c = std::max(c, c2);

            c = c * speed + (1 - speed) * weights[i];
            newWeights[i] = c;
        }
    }

    swap(weights, temporary_buffer);
}

void InfluenceMap::propagateSum(double decay, double speed, const InfluenceMap& traversable) {
    // double decayCorrectionFactor = (5*(1-decay) + 4*pow(1-decay,1.41))/9;
    // decay *= decay / (1 - decayCorrectionFactor);
    double factor = 1 - decay;
    // cout << "Estimated decay at " << ((5*(1-decay) + 4*pow(1-decay,1.41))/9) << endl;
    // Diagonal decay
    // double factor2 = pow(factor, 1.41);

    double gaussianFactor0 = 1;     //0.195346;
    double gaussianFactor1 = 1;     //0.123317;
    double gaussianFactor2 = 0.75;  //0.077847;

    temporary_buffer.resize(weights.size());
    vector<double>& newWeights = temporary_buffer;

    for (int y = 0; y < h; y++) {
        weights[y * w + 0] = weights[y * w + 1];
        weights[y * w + w - 1] = weights[y * w + w - 2];
    }
    for (int x = 0; x < w; x++) {
        weights[x] = weights[x + w];
        weights[(h - 1) * w + x] = weights[(h - 2) * w + x];
    }

    for (int y = 1; y < h - 1; y++) {
        int yw = y * w;
        for (int x = 1; x < w - 1; x++) {
            int i = yw + x;

            if (traversable[i] == 0) {
                newWeights[i] = 0;
                continue;
            }

            double neighbours = gaussianFactor0;
            neighbours += gaussianFactor1 * (traversable[i - 1] + traversable[i + 1] + traversable[i - w] + traversable[i + w]) + gaussianFactor2 * (traversable[i - w - 1] + traversable[i - w + 1] + traversable[i + w - 1] + traversable[i + w + 1]);

            double c = 0;
            c += weights[i - 1];
            c += weights[i + 1];
            c += weights[i - w];
            c += weights[i + w];
            c *= gaussianFactor1;

            c += weights[i] * gaussianFactor0;

            double c2 = 0;
            c2 += weights[i - w - 1];
            c2 += weights[i - w + 1];
            c2 += weights[i + w - 1];
            c2 += weights[i + w + 1];
            c2 *= gaussianFactor2;
            c += c2;

            // To prevent the total weight values from increasing unbounded
            if (neighbours > 0)
                c /= neighbours;

            c = c * speed + (1 - speed) * weights[i];
            newWeights[i] = c * factor;
        }
    }

    swap(weights, temporary_buffer);
}

void InfluenceMap::print() const {
    for (int y = 0; y < w; y++) {
        for (int x = 0; x < h; x++) {
            cout << setfill(' ') << setw(6) << setprecision(1) << fixed << weights[y * w + x] << " ";
        }
        cout << endl;
    }
}

static default_random_engine generator(time(0));

/** Sample a point from this influence map.
 * The probability of picking each cell is proportional to its weight.
 * The map does not have to be normalized.
 */
Point2DI InfluenceMap::samplePointFromProbabilityDistribution() const {
    uniform_real_distribution<double> distribution(0.0, sum());
    double picked = distribution(generator);

    for (int i = 0; i < w * h; i++) {
        picked -= weights[i];
        if (picked <= 0)
            return Point2DI(i % w, i / w);
    }

    // Should in theory not happen, but it may actually happen because of floating point errors
    return Point2DI(w - 1, h - 1);
}
