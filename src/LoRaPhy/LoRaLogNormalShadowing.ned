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

package flora.LoRaPhy;

import inet.physicallayer.wireless.common.pathloss.FreeSpacePathLoss;

module LoRaLogNormalShadowing extends FreeSpacePathLoss
{
    parameters:
        //parameters taken from paper LoRaSim
        double d0 = default(40m) @unit(m);
        double gamma = default(2.08);
        double sigma = default(3.57);
        // Ours: path loss at d0. FLoRa hardcodes 127.41 dB (with d0 = 40 m); our scenarios use
        // 103.139 dB at d0 = 100 m, the reference of ns-3's log-distance model.
        double PL_d0_db @unit(dB) = default(127.41dB);
        // Ours: Rayleigh fading. The received power is multiplied by an Exponential(1) draw per
        // frame (statistical Rayleigh channel, Coutaud et al., WiMob 2020).
        bool enableRayleighFading = default(false);
        // Ours: fixed-SNR mode. The distance is ignored and the path loss is set so that a device
        // transmitting fixedTxPowerDbm is received with a mean SNR of fixedMeanSnrDb above the
        // -117 dBm noise floor (SF12 sensitivity -137 dBm minus its -20 dB SNR floor). Shadowing
        // is not applied in this mode; Rayleigh fading is. nan (default) = off.
        double fixedMeanSnrDb = default(nan);
        double fixedTxPowerDbm = default(14);
        @class(LoRaLogNormalShadowing);
}
