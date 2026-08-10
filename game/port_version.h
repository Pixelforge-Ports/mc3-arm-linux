/*
 * The port's version - the single place it is written down.
 *
 * A field report is only actionable if it says which build produced it. Two
 * releases produce byte-identical logs otherwise, and a user reporting a
 * problem is running whatever is on their SD card, not necessarily what they
 * last downloaded.
 *
 * So the version is defined here and nowhere else:
 *
 *   - the loader prints it as its first trace line, and answers --version;
 *   - the launcher asks the binary (`mc3 --version`) instead of carrying its
 *     own copy, so a stale launcher cannot claim a version the binary is not;
 *   - the packager reads this header when it reports what it built.
 *
 * Bump it here when cutting a build; nothing else needs editing.
 */
#ifndef MC3_PORT_VERSION_H
#define MC3_PORT_VERSION_H

#define MC3_PORT_VERSION "1.0.1"

#endif /* MC3_PORT_VERSION_H */
