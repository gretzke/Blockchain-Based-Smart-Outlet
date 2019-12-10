pragma solidity ^0.5.0;

/**
 * @title SafeMath
 * @dev Unsigned math operations with safety checks that revert on error
 * @notice https://github.com/OpenZeppelin/openzeppelin-solidity
 */
library SafeMath {
    /**
     * @dev Multiplies two unsigned integers, reverts on overflow.
     */
    function mul(uint256 a, uint256 b) internal pure returns (uint256) {
        // Gas optimization: this is cheaper than requiring 'a' not being zero, but the
        // benefit is lost if 'b' is also tested.
        // See: https://github.com/OpenZeppelin/openzeppelin-solidity/pull/522
        if (a == 0) {
            return 0;
        }

        uint256 c = a * b;
        require(c / a == b);

        return c;
    }

    /**
     * @dev Integer division of two unsigned integers truncating the quotient, reverts on division by zero.
     */
    function div(uint256 a, uint256 b) internal pure returns (uint256) {
        // Solidity only automatically asserts when dividing by 0
        require(b > 0);
        uint256 c = a / b;
        // assert(a == b * c + a % b); // There is no case in which this doesn't hold

        return c;
    }

    /**
     * @dev Subtracts two unsigned integers, reverts on overflow (i.e. if subtrahend is greater than minuend).
     */
    function sub(uint256 a, uint256 b) internal pure returns (uint256) {
        require(b <= a);
        uint256 c = a - b;

        return c;
    }

    /**
     * @dev Adds two unsigned integers, reverts on overflow.
     */
    function add(uint256 a, uint256 b) internal pure returns (uint256) {
        uint256 c = a + b;
        require(c >= a);

        return c;
    }

    /**
     * @dev Divides two unsigned integers and returns the remainder (unsigned integer modulo),
     * reverts when dividing by zero.
     */
    function mod(uint256 a, uint256 b) internal pure returns (uint256) {
        require(b != 0);
        return a % b;
    }
}

contract SocketPaymentChannel {
    // use the SafeMath library for calculations with uint256 to prevent integer over- and underflows
    using SafeMath for uint256;

    address public owner;
    uint256 public pricePerSecond;

    // stores the balances of all customers and the owner
    mapping(address => uint256) public balances;

    // duration after a payment channel expires in seconds
    uint256 public expirationDuration;
    // minimum required deposit for payment channel
    uint256 public minDeposit;

    // global payment channel variables
    // boolean whether the payment channel is currently active
    bool public channelActive;
    // timestamp when payment channel was initialized
    uint256 public creationTimeStamp;
    // timestamp when payment channel will expire
    uint256 public expirationDate;
    // address of current customer
    address public channelCustomer;
    // value deposited into the smart contract
    uint256 public maxValue;

    // nonces to prevent replay attacks
    mapping(address => uint256) public customerNonces;

    // events
    event InitializedPaymentChannel(address indexed customer, uint256 indexed start, uint256 indexed maxValue, uint256 end);
    event ClosedPaymentChannel(address indexed sender, uint256 indexed value, bool indexed expired, uint256 duration);
    event PriceChanged(uint256 indexed oldPrice, uint256 indexed newPrice);
    event Withdrawal(address indexed sender, uint256 indexed amount);

    // modifier that only allows the owner to execute a function
    modifier onlyOwner() {
        require(msg.sender == owner, "sender is not owner");
        _;
    }

    constructor(uint256 _pricePerSecond, uint256 _expirationDuration, uint256 _minDeposit) public {
        owner = msg.sender;
        pricePerSecond = _pricePerSecond;
        expirationDuration = _expirationDuration;
        minDeposit = _minDeposit;
        channelActive = false;
    }

    /// @notice function to initialize a payment channel
    /// @return true on success, false on failure
    function initializePaymentChannel() public payable returns (bool) {
        // payment channel has to be inactive
        require(!channelActive, "payment channel already active");
        // value sent with the transaction has to be at least as much as the minimum required deposit
        require(msg.value >= minDeposit, "minimum deposit value not reached");

        // set global payment channel information
        channelActive = true;
        // set channel customer to the address of the caller of the transaction
        channelCustomer = msg.sender;
        // set the maximum transaction value to the deposited value
        maxValue = msg.value;
        // set the timestamp of the payment channel intialization
        creationTimeStamp = now;
        // calculate and set the expiration timestamp
        expirationDate = now.add(expirationDuration);

        // emit the initialization event
        // It's cheaper in gas to use msg.sender instead of loading the channelCustomer variable, msg.value instead of maxValue, etc.
        emit InitializedPaymentChannel(msg.sender, now, now.add(expirationDuration), msg.value);
        return true;
    }

    /// @notice function to close a payment channel and settle the transaction
    /// @dev can only be called by owner
    /// @param _value the total value of the payment channel
    /// @param _signature the signature of the last off-chain transaction containing the total value
    /// @return true on success, false on failure
    function closeChannel(uint256 _value, bytes memory _signature) public onlyOwner returns (bool) {
        // save value to a temporary variable, as it is reassigned later
        uint256 value = _value;
        // payment channel has to be active
        require(channelActive, "payment channel not active");
        // call verify signature function, if it returns false, the signature is invalid and the function throws
        require(verifySignature(value, _signature), "signature not valid");

        // increase nonce after payment channel is closed
        customerNonces[channelCustomer] = customerNonces[channelCustomer].add(1);

        // if maxValue was exceeded, set value to maxValue
        if (value > maxValue) {
            value = maxValue;
            // value of payment channel equals the exact deposited amount
            // credit owner the total value
            balances[owner] = balances[owner].add(value);
        } else {
            // credit owner the value from the payment channel
            balances[owner] = balances[owner].add(value);
            // refund the remaining value to the customer
            balances[channelCustomer] = balances[channelCustomer].add(maxValue.sub(value));
        }

        // emit payment channel closure event
        emit ClosedPaymentChannel(msg.sender, value, false, now - creationTimeStamp);

        // reset channel information
        channelActive = false;
        channelCustomer = address(0);
        maxValue = 0;
        expirationDate = 0;
        creationTimeStamp = 0;

        return true;
    }

    /// @notice function to timeout a payment channel, refunds entire deposited amount to customer
    /// @return true on success, false on failure
    function timeOutChannel() public returns (bool) {
        // payment channel has to be active
        require(channelActive, "payment channel not active");
        // payment channel has to be expired
        require(now > expirationDate, "payment channel not expired yet");

        // increase nonce after payment channel is closed
        customerNonces[channelCustomer] = customerNonces[channelCustomer].add(1);

        // return funds to customer if channel was not closed before channel expiration date
        balances[channelCustomer] = balances[channelCustomer].add(maxValue);

        // emit payment channel closure event
        emit ClosedPaymentChannel(msg.sender, 0, true, now - creationTimeStamp);

        // reset channel information
        channelActive = false;
        channelCustomer = address(0);
        maxValue = 0;
        expirationDate = 0;
        creationTimeStamp = 0;

        return true;
    }

    /// @notice helper function to validate off-chain transactions
    /// @param _value value of the off-chain transaction
    /// @param _signature signature / off-chain transaction
    /// @return true if the sender of the off-chain transaction is equal to the current customer, else false
    function verifySignature(uint256 _value, bytes memory _signature) public view returns (bool) {

        // split signature into r,s,v values (https://programtheblockchain.com/posts/2018/02/17/signing-and-verifying-messages-in-ethereum/)
        require(_signature.length == 65, "signature length is not 65 bytes");

        bytes32 r;
        bytes32 s;
        uint8 v;

        // split using inline assembly
        assembly {
            // first 32 bytes of message
            r := mload(add(_signature, 32))
            // second 32 bytes of message
            s := mload(add(_signature, 64))
            // first byte of the next 32 bytes
            v := byte(0, mload(add(_signature, 96)))
        }

        // to recover the address of the sender, the signed data has to be recreated
        // variables that are included in the message: value, address of contract, nonce of customer
        address contractAddress = address(this);
        // hash variables
        bytes32 message = keccak256(abi.encodePacked(_value, contractAddress, customerNonces[channelCustomer]));
        // prefix message with ethereum specific prefix
        bytes32 prefixedMessage = keccak256(abi.encodePacked("\x19Ethereum Signed Message:\n32", message));
        // ecrecover recovers the address from a signature and the signed data
        // returns true if recovered address is equal to customer address
        return ecrecover(prefixedMessage, v, r, s) == channelCustomer;
    }

    /// @notice determine whether the current payment channel has expired
    /// @return true if channel has expired
    function channelExpired() public view returns(bool) {
        if (channelActive) {
            return now > expirationDate;
        }
        return false;
    }

    /// @notice function to withdraw funds from the smart contract
    /// @return true on success, false on failure
    function withdraw() public returns (bool) {
        // save balance of sender to a variable
        uint256 balance = balances[msg.sender];
        // best practice: set balance of sender to zero before sending the transaction, see reentrancy attack
        balances[msg.sender] = 0;
        // send entire balance to sender
        msg.sender.transfer(balance);
        // emit withdrawal event
        emit Withdrawal(msg.sender, balance);
        return true;
    }

    /// @notice function to change the electricity price, only callable by the owner
    function changePrice(uint256 _newPrice) public onlyOwner {
        // emit price changed event
        emit PriceChanged(pricePerSecond, _newPrice);
        // update price per second
        pricePerSecond = _newPrice;
    }
}