# Contributing to the Firestorm VR Mod

Thank you for your interest in contributing to the Firestorm VR Mod! This is a community-driven project, and we welcome any help we can get. However, it's important to understand that this is not a typical open-source project. Due to the nature of the mod and its relationship with the upstream Firestorm viewer, our contribution process is a bit different.

## A Different Kind of Open Source

The Firestorm VR Mod is intentionally designed as a minimal, additive patch set to the official Firestorm viewer. This approach has several important implications:

- **No Full Fork:** We do not maintain a full fork of the Firestorm viewer. This would be a massive undertaking and would make it very difficult to keep up with the rapid pace of upstream development.
- **Minimal Changes:** We strive to keep the changes to the upstream codebase as small and as isolated as possible. This minimizes the risk of merge conflicts when a new version of Firestorm is released.
- **Focus on Stability:** Our primary goal is to provide a stable and reliable VR experience. We are very cautious about adding new features that could introduce instability or create a maintenance burden.

Because of these constraints, we cannot accept large, complex pull requests or major new features. However, there are still many ways you can contribute to the project.

## Getting Started

The biggest hurdle to contributing to the VR mod is setting up a build environment. The Firestorm viewer is a large and complex application, and compiling it from source can be a challenge.

1. **Follow the Official Guide:** The first step is to follow the official Firestorm guide for compiling the viewer from source. You can find the guide on the [Firestorm Wiki](https://wiki.firestormviewer.org/fs_compiling_firestorm).
2. **Apply the VR Mod:** Once you have a working build environment, you can apply the VR mod by following the instructions in the [`readme.md`](./readme.md) file.
3. **Join the Community:** The best place to get help and to discuss your ideas is the [P373R VR Mod Discord server](https://discordapp.com/channels/613119193734316042/613119193734316044) [ [Invite](https://discord.com/invite/QMJ9vCA) ].

## Good First Issues

If you're new to the project, here are some ideas for small, well-defined tasks that would be a great way to get started:

- **Improve Code Comments:** The `llviewerVR.cpp` and `llviewerVR.h` files could always use more comments to explain the code's functionality.
- **Refactor for Clarity:** There are many opportunities to refactor small sections of the code to improve its readability and maintainability without changing its functionality.
- **Fix Minor Bugs:** If you've encountered a small bug, and you have an idea for how to fix it, we'd love to see your solution.

## The "Finish Line" Strategy

We want to provide a clear path for contributors to get their changes integrated into a community build. Here is our process:

1. **Fork and Develop:** Fork this repository and make your changes in your own fork.
2. **Compile and Test:** Compile your changes and test them thoroughly. Make sure that your changes do not introduce any new bugs or regressions.
3. **Share and Gather Feedback:** Share your custom build with the community in the Discord channel. This is a crucial step. We need to see that your changes are stable and that they are well-received by the community.
4. **Submit a Pull Request:** Once your changes have been vetted by the community, you can submit a pull request to this repository. Please provide a clear and detailed description of your changes and why you think they should be included.

Please understand that we cannot promise to merge every pull request. We have to be very selective about the changes we accept to ensure the long-term stability and maintainability of the mod. However, by following this process, you can give your changes the best possible chance of being included in a future community release.

Thank you again for your interest in contributing to the Firestorm VR Mod. We look forward to seeing what you can do!

