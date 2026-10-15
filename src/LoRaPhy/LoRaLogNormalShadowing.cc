//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

#include "LoRaLogNormalShadowing.h"
#include "inet/common/INETMath.h"

namespace flora {

Define_Module(LoRaLogNormalShadowing);

LoRaLogNormalShadowing::LoRaLogNormalShadowing()
{
}

void LoRaLogNormalShadowing::initialize(int stage)
{
    FreeSpacePathLoss::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        sigma = par("sigma");
        gamma = par("gamma");
        d0 = m(par("d0"));
        PL_d0_db = par("PL_d0_db");
        enableRayleighFading = par("enableRayleighFading");
        fixedMeanSnrDb = par("fixedMeanSnrDb");
        fixedTxPowerDbm = par("fixedTxPowerDbm");
    }
}

std::ostream& LoRaLogNormalShadowing::printToStream(std::ostream& stream, int level, int evFlags) const
{
    stream << "LoRaLogNormalShadowing";
    if (level <= PRINT_LEVEL_TRACE)
        stream << ", alpha = " << alpha
               << ", systemLoss = " << systemLoss
               << ", sigma = " << sigma;
    return stream;
}

double LoRaLogNormalShadowing::computePathLoss(mps propagationSpeed, Hz frequency, m distance) const
{
    double PL_db;
    if (std::isnan(fixedMeanSnrDb)) {
        // parameters taken from paper "Do LoRa Low-Power Wide-Area Networks Scale?"
        // (ours: PL_d0_db is a parameter, 127.41 dB by default)
        PL_db = PL_d0_db + 10 * gamma * log10(unit(distance / d0).get()) + normal(0.0, sigma);
    }
    else {
        // ours: fixed-SNR mode, mean SNR fixedMeanSnrDb above the -117 dBm noise floor
        const double noiseFloorDbm = -117.0;
        PL_db = fixedTxPowerDbm - (noiseFloorDbm + fixedMeanSnrDb);
    }
    if (enableRayleighFading) {
        // ours: |h|^2 ~ Exp(1), an additive 10 log10 |h|^2 dB term on the received power
        double h2 = exponential(1.0);
        if (h2 < 1e-20)
            h2 = 1e-20;   // guard against -inf dB
        PL_db -= 10.0 * log10(h2);
    }
    return math::dB2fraction(-PL_db);
}

m LoRaLogNormalShadowing::computeRange(W transmissionPower) const
{
    // parameters taken from paper "Do LoRa Low-Power Wide-Area Networks Scale?"
    // (ours: PL_d0_db is a parameter, 127.41 dB by default)
    double max_sensitivity = -137;
    double trans_power_db = round(10 * log10(transmissionPower.get()*1000));
    EV << "LoRaLogNormalShadowing transmissionPower in W = " << transmissionPower << " in dBm = " << trans_power_db << endl;
    double rhs = (trans_power_db - PL_d0_db - max_sensitivity)/(10 * gamma);
    double distance = d0.get() * pow(10, rhs);
    return m(distance);
}

}
