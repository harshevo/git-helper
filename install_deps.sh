#!/bin/bash
#
# install_deps.sh - Install dependencies for Git Master
#
# This script installs raylib and raygui for GUI support,
# as well as libnotify for desktop notifications.
#

set -e

echo "╔══════════════════════════════════════════════════════════╗"
echo "║     Git Master - Dependency Installer                    ║"
echo "╚══════════════════════════════════════════════════════════╝"
echo ""

# Detect the distribution
detect_distro() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        DISTRO=$ID
        DISTRO_LIKE=$ID_LIKE
    elif [ -f /etc/lsb-release ]; then
        . /etc/lsb-release
        DISTRO=$DISTRIB_ID
    elif [ -f /etc/debian_version ]; then
        DISTRO="debian"
    elif [ -f /etc/redhat-release ]; then
        DISTRO="rhel"
    elif [ -f /etc/arch-release ]; then
        DISTRO="arch"
    else
        DISTRO="unknown"
    fi
    
    echo "Detected distribution: $DISTRO"
}

# Install on Debian/Ubuntu
install_debian() {
    echo ""
    echo "Installing dependencies for Debian/Ubuntu..."
    echo ""
    
    # Update package list
    sudo apt update
    
    # Install build essentials
    echo "[1/5] Installing build tools..."
    sudo apt install -y build-essential git cmake
    
    # Install libnotify
    echo "[2/5] Installing libnotify for notifications..."
    sudo apt install -y libnotify-dev libnotify4
    
    # Check if raylib is available in repos
    if apt-cache show libraylib-dev &>/dev/null; then
        echo "[3/5] Installing raylib from repositories..."
        sudo apt install -y libraylib-dev
    else
        echo "[3/5] Raylib not in repos, building from source..."
        install_raylib_source
    fi
    
    # Install raygui header
    echo "[4/5] Installing raygui header..."
    install_raygui_header
    
    echo "[5/5] Done!"
}

# Install on Arch Linux
install_arch() {
    echo ""
    echo "Installing dependencies for Arch Linux..."
    echo ""
    
    # Install build essentials
    echo "[1/5] Installing build tools..."
    sudo pacman -S --needed --noconfirm base-devel git cmake
    
    # Install libnotify
    echo "[2/5] Installing libnotify..."
    sudo pacman -S --needed --noconfirm libnotify
    
    # Install raylib
    echo "[3/5] Installing raylib..."
    sudo pacman -S --needed --noconfirm raylib
    
    # Install raygui header
    echo "[4/5] Installing raygui header..."
    install_raygui_header
    
    echo "[5/5] Done!"
}

# Install on Fedora/RHEL
install_fedora() {
    echo ""
    echo "Installing dependencies for Fedora/RHEL..."
    echo ""
    
    # Install build essentials
    echo "[1/5] Installing build tools..."
    sudo dnf install -y gcc gcc-c++ make git cmake
    
    # Install libnotify
    echo "[2/5] Installing libnotify..."
    sudo dnf install -y libnotify libnotify-devel
    
    # Check if raylib is available
    if dnf list raylib-devel &>/dev/null 2>&1; then
        echo "[3/5] Installing raylib from repositories..."
        sudo dnf install -y raylib-devel
    else
        echo "[3/5] Raylib not in repos, building from source..."
        install_raylib_source
    fi
    
    # Install raygui header
    echo "[4/5] Installing raygui header..."
    install_raygui_header
    
    echo "[5/5] Done!"
}

# Build raylib from source
install_raylib_source() {
    echo ""
    echo "Building raylib from source..."
    echo "This may take a few minutes..."
    echo ""
    
    # Create temp directory
    TEMP_DIR=$(mktemp -d)
    cd "$TEMP_DIR"
    
    # Clone raylib
    echo "Cloning raylib repository..."
    git clone --depth 1 https://github.com/raysan5/raylib.git
    cd raylib/src
    
    # Build
    echo "Building raylib..."
    make PLATFORM=PLATFORM_DESKTOP
    
    # Install
    echo "Installing raylib..."
    sudo make install RAYLIB_INSTALL_PATH=/usr/local RAYLIB_H_INSTALL_PATH=/usr/local/include
    
    # Update library cache
    sudo ldconfig
    
    # Cleanup
    cd ~
    rm -rf "$TEMP_DIR"
    
    echo "Raylib installed successfully!"
}

# Install raygui header
install_raygui_header() {
    echo ""
    echo "Installing raygui header..."
    
    # Download raygui.h
    RAYGUI_URL="https://raw.githubusercontent.com/raysan5/raygui/master/src/raygui.h"
    
    # Try to install to system include path
    if sudo curl -sL "$RAYGUI_URL" -o /usr/local/include/raygui.h 2>/dev/null; then
        echo "Installed raygui.h to /usr/local/include/"
    else
        # Fall back to project directory
        curl -sL "$RAYGUI_URL" -o raygui.h
        echo "Downloaded raygui.h to current directory"
        echo "Note: The header is in your project folder"
    fi
}

# Install only notifications (no GUI)
install_notifications_only() {
    echo ""
    echo "Installing notification support only (no GUI)..."
    echo ""
    
    detect_distro
    
    case $DISTRO in
        ubuntu|debian|linuxmint|pop)
            sudo apt update
            sudo apt install -y libnotify-dev libnotify4
            ;;
        arch|manjaro|endeavouros)
            sudo pacman -S --needed --noconfirm libnotify
            ;;
        fedora|rhel|centos|rocky|almalinux)
            sudo dnf install -y libnotify libnotify-devel
            ;;
        opensuse*|suse*)
            sudo zypper install -y libnotify libnotify-devel
            ;;
        *)
            echo "Unknown distribution. Please install libnotify manually."
            exit 1
            ;;
    esac
    
    echo ""
    echo "Notifications support installed!"
    echo "You can now build with: make"
}

# Main menu
show_menu() {
    echo "What would you like to install?"
    echo ""
    echo "  1. Full installation (raylib GUI + notifications)"
    echo "  2. Notifications only (libnotify, no GUI)"
    echo "  3. Build raylib from source only"
    echo "  4. Install raygui header only"
    echo "  5. Check current installation"
    echo "  0. Exit"
    echo ""
    read -p "Enter choice [0-5]: " choice
    
    case $choice in
        1)
            detect_distro
            case $DISTRO in
                ubuntu|debian|linuxmint|pop)
                    install_debian
                    ;;
                arch|manjaro|endeavouros)
                    install_arch
                    ;;
                fedora|rhel|centos|rocky|almalinux)
                    install_fedora
                    ;;
                *)
                    echo "Unknown distribution: $DISTRO"
                    echo "Attempting generic source installation..."
                    install_raylib_source
                    install_raygui_header
                    ;;
            esac
            ;;
        2)
            install_notifications_only
            ;;
        3)
            install_raylib_source
            ;;
        4)
            install_raygui_header
            ;;
        5)
            check_installation
            ;;
        0)
            echo "Exiting."
            exit 0
            ;;
        *)
            echo "Invalid choice"
            exit 1
            ;;
    esac
}

# Check what's installed
check_installation() {
    echo ""
    echo "Checking installation status..."
    echo ""
    
    # Check GCC
    if command -v gcc &>/dev/null; then
        echo "✓ GCC: $(gcc --version | head -1)"
    else
        echo "✗ GCC: NOT FOUND"
    fi
    
    # Check Git
    if command -v git &>/dev/null; then
        echo "✓ Git: $(git --version)"
    else
        echo "✗ Git: NOT FOUND"
    fi
    
    # Check libnotify
    if ldconfig -p 2>/dev/null | grep -q libnotify; then
        echo "✓ libnotify: INSTALLED"
    elif pkg-config --exists libnotify 2>/dev/null; then
        echo "✓ libnotify: INSTALLED"
    else
        echo "✗ libnotify: NOT FOUND (notifications disabled)"
    fi
    
    # Check raylib
    if ldconfig -p 2>/dev/null | grep -q libraylib; then
        echo "✓ raylib: INSTALLED"
    elif pkg-config --exists raylib 2>/dev/null; then
        echo "✓ raylib: $(pkg-config --modversion raylib)"
    elif [ -f /usr/local/lib/libraylib.a ] || [ -f /usr/lib/libraylib.a ]; then
        echo "✓ raylib: INSTALLED (static)"
    else
        echo "✗ raylib: NOT FOUND (GUI disabled)"
    fi
    
    # Check raygui
    if [ -f /usr/local/include/raygui.h ] || [ -f /usr/include/raygui.h ] || [ -f raygui.h ]; then
        echo "✓ raygui: HEADER FOUND"
    else
        echo "✗ raygui: HEADER NOT FOUND"
    fi
    
    echo ""
    echo "Build commands:"
    echo "  make           - Build CLI only (works without raylib)"
    echo "  make gui       - Build with GUI (requires raylib + raygui)"
    echo ""
}

# Parse command line arguments
if [ "$1" == "--check" ]; then
    check_installation
    exit 0
elif [ "$1" == "--notifications" ]; then
    install_notifications_only
    exit 0
elif [ "$1" == "--full" ]; then
    detect_distro
    case $DISTRO in
        ubuntu|debian|linuxmint|pop)
            install_debian
            ;;
        arch|manjaro|endeavouros)
            install_arch
            ;;
        fedora|rhel|centos|rocky|almalinux)
            install_fedora
            ;;
        *)
            echo "Unknown distribution, trying source build..."
            install_raylib_source
            install_raygui_header
            ;;
    esac
    exit 0
elif [ "$1" == "--help" ] || [ "$1" == "-h" ]; then
    echo "Usage: $0 [OPTION]"
    echo ""
    echo "Options:"
    echo "  --full          Install everything (raylib + notifications)"
    echo "  --notifications Install notifications only (no GUI)"
    echo "  --check         Check current installation status"
    echo "  --help          Show this help"
    echo ""
    echo "Without options, shows interactive menu."
    exit 0
fi

# Show interactive menu
show_menu

echo ""
echo "════════════════════════════════════════════════════════════"
echo ""
echo "Installation complete!"
echo ""
echo "Next steps:"
echo "  1. Build the CLI:  make"
echo "  2. Build with GUI: make gui"
echo "  3. Run:            ./git_master"
echo ""
echo "If raylib was installed to /usr/local, you may need to:"
echo "  export LD_LIBRARY_PATH=/usr/local/lib:\$LD_LIBRARY_PATH"
echo ""
