nume_fisier = 'date_temperaturi_curat.csv';
interval_celule = 'A1:A1671'; % The data is in the range A1:A1671
temperatura = xlsread(nume_fisier, interval_celule);
temperatura = temperatura(~isnan(temperatura) & ~isinf(temperatura));
% Creating the time vector
timp = (0:0.5:(length(temperatura)-1)*0.5)';
duty_cycle = 90; % The PWM from the Arduino code
s = tf('s');
t = 700; % transient time in seconds
%k = (60 / duty_cycle);
H = ((80 - 20) / duty_cycle) / (t * s + 1); % 65 initial % 20 degrees - ambient temperature
step(duty_cycle * H); % step response for the equivalent transfer function identified by us - experimental
hold on;
plot(timp, temperatura-20, 'r'); % the real response to a step (duty_cycle) of 90 PWM