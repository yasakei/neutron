#!/bin/bash
# Check LSP server compatibility and fix common issues

set -e

echo "Neutron LSP Compatibility Checker"
echo "================================="

# Find neutron-lsp binary
LSP_BINARY=""
if [ -f "./neutron-lsp" ]; then
    LSP_BINARY="./neutron-lsp"
elif [ -f "./build/neutron-lsp" ]; then
    LSP_BINARY="./build/neutron-lsp"
elif command -v neutron-lsp >/dev/null 2>&1; then
    LSP_BINARY="neutron-lsp"
else
    echo "❌ neutron-lsp binary not found"
    echo "Please build it first or ensure it's in your PATH"
    exit 1
fi

echo "✓ Found neutron-lsp at: $LSP_BINARY"

# Check dependencies
echo ""
echo "Checking dependencies..."
if ldd "$LSP_BINARY" >/dev/null 2>&1; then
    MISSING_DEPS=$(ldd "$LSP_BINARY" | grep "not found" || true)
    if [ -n "$MISSING_DEPS" ]; then
        echo "❌ Missing dependencies found:"
        echo "$MISSING_DEPS"
        
        # Check for jsoncpp version mismatch
        if echo "$MISSING_DEPS" | grep -q "libjsoncpp.so.25"; then
            echo ""
            echo "🔧 Detected jsoncpp version mismatch. Attempting to fix..."
            
            # Find available jsoncpp versions
            JSONCPP_AVAILABLE=$(ldconfig -p | grep libjsoncpp.so | head -1 | awk '{print $1}' || true)
            if [ -n "$JSONCPP_AVAILABLE" ]; then
                JSONCPP_PATH=$(ldconfig -p | grep "$JSONCPP_AVAILABLE" | awk '{print $4}')
                JSONCPP_DIR=$(dirname "$JSONCPP_PATH")
                
                echo "Found available jsoncpp: $JSONCPP_PATH"
                echo "Creating compatibility symlink..."
                
                if sudo ln -sf "$JSONCPP_PATH" "$JSONCPP_DIR/libjsoncpp.so.25" 2>/dev/null; then
                    echo "✓ Created jsoncpp compatibility symlink"
                    
                    # Test again
                    MISSING_DEPS_AFTER=$(ldd "$LSP_BINARY" | grep "not found" || true)
                    if [ -z "$MISSING_DEPS_AFTER" ]; then
                        echo "✓ All dependencies resolved!"
                    else
                        echo "❌ Some dependencies still missing:"
                        echo "$MISSING_DEPS_AFTER"
                    fi
                else
                    echo "❌ Could not create symlink automatically"
                    echo "Manual fix: sudo ln -sf $JSONCPP_PATH $JSONCPP_DIR/libjsoncpp.so.25"
                fi
            else
                echo "❌ No jsoncpp library found on system"
                echo "Install jsoncpp: sudo apt-get install libjsoncpp-dev"
            fi
        fi
    else
        echo "✓ All dependencies satisfied"
    fi
else
    echo "✓ Static binary (no dynamic dependencies)"
fi

# Test basic functionality
echo ""
echo "Testing LSP server functionality..."
if timeout 5s echo '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"capabilities":{}}}' | "$LSP_BINARY" >/dev/null 2>&1; then
    echo "✓ LSP server responds to initialize request"
else
    echo "❌ LSP server failed to respond properly"
    echo "This might indicate a protocol implementation issue"
fi

echo ""
echo "Compatibility check complete!"