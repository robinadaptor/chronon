Chronon Core version *1.6.4* is now available from:  <https://github.com/robinadaptor/chronon/releases>

This is a new major version release, including various bug fixes and performance improvements.

Please report bugs using the issue tracker at github: <https://github.com/robinadaptor/chronon/issues>


Mandatory Update
==============

Chronon Core v1.6.4 is a mandatory update for all users. This release contains new consensus rules and improvements that are not backwards compatible with older versions. Users will need to update their clients before enforcement of this update goes into effect.

Update enforcement goes into effect at the following times:

    Testnet: Wednesday, Sep 18, 2019 7:00:00 PM GMT
    Mainnet: Wednesday, Sep 18, 2019 4:00:00 PM GMT

Masternodes will need to be restarted once both the masternode daemon and the controller wallet have been upgraded.

How to Upgrade
==============

If you are running an older version, shut it down. Wait until it has completely shut down (which might take a few minutes for older versions), then run the installer (on Windows) or just copy over /Applications/Chronon-Qt (on Mac) or chronond/chronon-qt (on Linux).


Compatibility
==============

Chronon Core is extensively tested on multiple operating systems using the Linux kernel, macOS 10.10+, and Windows 7 and later.

Microsoft ended support for Windows XP on [April 8th, 2014](https://www.microsoft.com/en-us/WindowsForBusiness/end-of-xp-support), No attempt is made to prevent installing or running the software on Windows XP, you can still do so at your own risk but be aware that there are known instabilities and issues. Please do not report issues about Windows XP to the issue tracker.

Apple released it's last Mountain Lion update August 13, 2015, and officially ended support on [December 14, 2015](http://news.fnal.gov/2015/10/mac-os-x-mountain-lion-10-8-end-of-life-december-14/). Chronon Core software starting with v3.2.0 will no longer run on MacOS versions prior to Yosemite (10.10). Please do not report issues about MacOS versions prior to Yosemite to the issue tracker.

Chronon Core should also work on most other Unix-like systems but is not frequently tested on them.


Notable Changes
==============

## Internal (Core) Changes

### Version 2 Stake Modifier

A new 256-bit modifier for the proof of stake protocol has been defined, `CBlockIndex::nStakeModifierV2`.
It is computed at every block, by taking the hash of the modifier of previous block along with the coinstake input.
To meet the protocol, the PoS kernel must comprise the modifier of the previous block.

Changeover enforcement of this new modifier is set to occur at block `1214000` for testnet and block `1967000` for mainnet.

### Block index batch writing

Block index writes are now done in a batch. This allows for less frequent disk access, meaning improved performances and less data corruption risks.

### Eliminate needless key generation

The staking process has been improved to no longer request a new (unused) key from the keypool. This should reduce wallet file size bloat as well as slightly improve staking efficiency.

### Fix crash scenario at wallet startup

A program crash bug that happens when the wallet.dat file contains a zc public spend transaction (input) and the user had removed the chain data has been fixed.

## GUI Changes

### Removal of zero-fee transaction option

The long term viability of acceptable zero-fee transaction conditions is in need of review. As such, we are temporarily disabling the ability to create zero-fee transactions.

### Show latest block hash and datadir information tab

A QoL addition has been made to the Information tab of the UI's console window, which adds the display of both the current data directory and the latest block hash seen by the client.

## RPC Changes

### Require valid URL scheme when preparing/submitting a proposal

The `preparebudget` and `submitbudget` RPC commands now require the inclusion of a canonical URL scheme as part of their `url` parameter. Strings that don't include either `http://` or `https://` will be rejected.

The 64 character limit for the `url` field is inclusive of this change, so the use of a URL shortening service may be needed.

## Testing Suite Changes

### Functional testing readability

Several changes have been introduced to the travis script in order to make the output more readable. Specifically it now lists tests left to run and prints the output of failing scripts.

## Build System Changes

### OpenSSL configure information

When the configure step fails because of an unsupported OpenSSL (or other library), it now displays more information on using an override flag to compile anyways. The long term plan is to ensure that the consensus code doesn't depend on OpenSSL in any way and then remove this configure step and related override flag.

