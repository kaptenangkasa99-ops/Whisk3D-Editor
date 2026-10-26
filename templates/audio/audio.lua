local frequency = 440

function update(dt)
    if isPressed(object("PlayTone")) or keyPressed("space") then
        beep(frequency, 180, 0.7)
        frequency = frequency + 110
        if frequency > 880 then frequency = 440 end
    end
end
